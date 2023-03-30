#include "NetworkCommunicator.h"

#include <iostream>

#include <boost/serialization/vector.hpp>
#include <boost/serialization/shared_ptr.hpp>

#include <boost/archive/text_oarchive.hpp>
#include <boost/archive/text_oarchive.hpp>

#include "ExternalMixerHandler.h"
#include "../PluginProcessor.h"

#include "../TypesForDataExchange.h"

using namespace GA;

#include <message_protocol/Network/ConnectionRequestMessage.h>
BOOST_CLASS_EXPORT(ConnectionRequestMessage)

#include <message_protocol/Control/PannerSettingsMessage.h>
BOOST_CLASS_EXPORT(PannerSettingsMessage)

#include <message_protocol/Control/PannerTrackNameMessage.h>
BOOST_CLASS_EXPORT(PannerTrackNameMessage)

#include <message_protocol/Control/TooltipUpdateMessage.h>
BOOST_CLASS_EXPORT(TooltipUpdateMessage)

#include "message_protocol/Control/ExternalMixerSettingsMessage.h"
BOOST_CLASS_EXPORT(ExternalMixerSettingsMessage)

using namespace boost::asio;

auto ConvertPannerSettings(::PannerSettings& curr_set, PannerSettingsParameters& settings)
{
	settings.inputType = curr_set.m1Encode.getInputMode();
	settings.outputType = curr_set.m1Encode.getOutputMode();

	/**
	 if (pannerSettings.isotropicMode) {
			if (pannerSettings.equalpowerMode) {
				pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::Mach1EncodePannerModeIsotropicEqualPower);
			}
else {
	pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::Mach1EncodePannerModeIsotropicLinear);
}
}
else {
	pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::Mach1EncodePannerModePeriphonicLinear);
}
		 */

		 //settings.pannerMode = ;

	settings.x = curr_set.x;
	settings.y = curr_set.y;
	settings.azimuth = curr_set.azimuth;
	settings.elevation = curr_set.elevation;
	settings.diverge = curr_set.diverge;
	settings.gain = curr_set.gain;

	settings.stereoOrbitAzimuth = curr_set.stereoOrbitAzimuth;
	settings.stereoSpread = curr_set.stereoSpread;
	settings.stereoInputBalance = curr_set.stereoInputBalance;
	settings.autoOrbit = curr_set.autoOrbit;
	settings.overlay = curr_set.overlay;
	settings.isotropicMode = curr_set.isotropicMode;
	settings.equalpowerMode = curr_set.equalpowerMode;
}

NetworkCommunicator::NetworkCommunicator(ExternalMixerHandler& owner, const std::string& host, const std::string& service)
	: m_mixer_handler(owner), m_connection_handler(boost::asio::make_strand(m_io_ctx.get_executor())), m_instance_index(0), m_reconnect_timer(m_io_ctx)
{
	ip::tcp::resolver resolver(m_io_ctx);
	m_endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(host, service));
}

NetworkCommunicator::~NetworkCommunicator()
{
	boost::system::error_code error;
	m_connection_handler.GetSocket().shutdown(socket_base::shutdown_both, error);
	m_connection_handler.GetSocket().close();
}

void NetworkCommunicator::Connect()
{
	m_connection_handler.GetSocket().async_connect(m_endpoint_iterator->endpoint(),
		[this](const boost::system::error_code& e) {
			if (!e) {
				OnSuccessfulConnection(e);
			}
			else
			{
				if (e == boost::asio::error::already_connected)
				{
					m_connection_handler.GetSocket() = boost::asio::ip::tcp::socket(m_io_ctx);
				}

				// & if connection refused

				HandleReconnect(e);
			}
		});
}

void NetworkCommunicator::Start()
{
	m_io_ctx.run();
}

void NetworkCommunicator::Stop()
{
	m_connection_handler.GetSocket().close();
	m_io_ctx.stop();
}

bool NetworkCommunicator::ReadyToSend()
{
	return m_ready_to_send;
}

void NetworkCommunicator::SetInstanceInfo(std::string session, int instance_index)
{
	m_session_id = session;
	m_instance_index = instance_index;
}

void NetworkCommunicator::UpdateTrackInfo(std::string track_name, std::string colour)
{
	std::scoped_lock slock(message_send_mutex);
	PannerTrackNameParameters params{ m_instance_index, track_name, colour };
	m_outgoing_messages.emplace(boost::make_shared<PannerTrackNameMessage>(params));
	SendNetworkMessage(m_outgoing_messages.back());
}

void NetworkCommunicator::OnSuccessfulConnection(const boost::system::error_code& e)
{
	// When we've successfully connected, we send our instance index and session ID
	// We then wait to read for an Ack response, telling us that the external mixer has initialized its data
	if (!e)
	{
		m_outgoing_messages.emplace(boost::make_shared<ConnectionRequestMessage>
			(ConnectionRequestParameters{ m_session_id, m_instance_index }));

		m_connection_handler.async_write(m_outgoing_messages.back(), boost::bind(
			&NetworkCommunicator::DoRead,
			this,
			boost::asio::placeholders::error));

		m_ready_to_send = true;
	}
	else {
		std::cout << e.message() << std::endl;
	}
}

void NetworkCommunicator::DoRead(const boost::system::error_code& e)
{
	if (!e) {
		m_incoming_message = boost::make_shared<GA::Message>();
		m_connection_handler.async_read(m_incoming_message,
			[this](const boost::system::error_code& e) {
				if (!e)
				{
					this->HandleRead(e);
				}
				else
				{
					std::cout << e.what() << std::endl;
				}
			});
	}
}

void NetworkCommunicator::HandleIncomingMessage(const boost::shared_ptr<GA::Message>& message)
{
	auto& owner = m_mixer_handler.GetOwner();

	switch (message->GetMessageId())
	{
	case GA::Ack: {
		// We've been acknowledged, respond with the current instance Panner Settings
		PannerSettingsParameters settings{};
		auto& curr_set = m_mixer_handler.GetOwner().pannerSettings;
		ConvertPannerSettings(curr_set, settings);

		const auto track_settings = owner.GetTrackProperties();
		settings.track_name = track_settings.name.toStdString();
		m_outgoing_messages.emplace(boost::make_shared<GA::PannerSettingsMessage>(settings, false, ePlugin));
		SendNetworkMessage(m_outgoing_messages.back());

		break;
	}
	case GA::PannerSettings:
	{
		auto data = boost::dynamic_pointer_cast<GA::PannerSettingsMessage>(message)->GetData();
		auto& curr_set = owner.pannerSettings;

		if (owner.getValueTreeState().getParameter(owner.paramElevation)->getValue() != data.elevation)
		{
			//owner.parameterChanged(owner.paramElevation, curr_set.elevation);
			owner.pannerSettings.elevation = curr_set.elevation;
		}
		if (owner.getValueTreeState().getParameter(owner.paramGain)->getValue() != data.gain)
		{
			owner.pannerSettings.gain = curr_set.gain;
			//owner.parameterChanged(owner.paramGain, curr_set.gain);
		}

		break;
	}
	case GA::ExternalMixerSettings:
	{
		auto data = boost::dynamic_pointer_cast<GA::ExternalMixerSettingsMessage>(message)->GetData();
		auto& curr_set = owner.monitorSettings;
		curr_set.pitch = data.pitch;
		curr_set.yaw = data.yaw;
		curr_set.roll = data.roll;

		curr_set.monitor_output_channel_count = data.monitor_output_channel_count;
		curr_set.monitor_input_channel_count = data.monitor_input_channel_count;

		break;
	}
	default:
	{
		break;
	};
	}
}

void NetworkCommunicator::HandleRead(const boost::system::error_code& e)
{
	if (!e) {
		HandleIncomingMessage(m_incoming_message);
	}
	else {
		if (e == error::connection_reset || e == error::eof) {
			// We've gotten disconnected...
			// For now, we'll just try to reconnect in regular intervals
			HandleReconnect(e);
		}
		std::cout << e.message() << std::endl;
	}

	m_connection_handler.async_read(m_incoming_message,
		[this](const boost::system::error_code& e) {
			this->HandleReadTimeout(e);
		});
}

void NetworkCommunicator::HandleReadTimeout(const boost::system::error_code& e)
{
	if (!e) {
		m_connection_handler.async_read(m_incoming_message,
			[this](const boost::system::error_code& e) {
				this->HandleRead(e);
			});
	}
	else {
		std::cout << e.message() << std::endl;
	}
}

void NetworkCommunicator::SendNetworkMessage(const boost::shared_ptr<GA::Message>& message)
{
	std::scoped_lock slock(message_send_mutex);

	m_connection_handler.async_write(message,
		boost::bind(&NetworkCommunicator::HandleWrite,
			this, boost::asio::placeholders::error));
}

// Really hate to use global namespace. We should add a MACH1 namespace
void NetworkCommunicator::UpdateInstance(::PannerSettings& settings)
{
	PannerSettingsParameters parameters;
	ConvertPannerSettings(settings, parameters);

	constexpr auto is_update_message = true;
	//m_incoming_message = boost::make_shared<PannerSettingsMessage>(parameters, is_update_message);

	{
		std::scoped_lock slock(message_send_mutex);
		m_outgoing_messages.emplace(boost::make_shared<PannerSettingsMessage>(parameters,
			is_update_message,
			MessageOrigin::ePlugin));

		SendNetworkMessage(m_outgoing_messages.back());
	}
}

void NetworkCommunicator::UpdateTooltip(std::string message, int timeout)
{
	TooltipUpdateParameters data{ message, timeout };
	//m_incoming_message = boost::make_shared<TooltipUpdateMessage>(data);
	{
		std::scoped_lock slock(message_send_mutex);
		m_outgoing_messages.emplace(boost::make_shared<TooltipUpdateMessage>(data));
	}
	SendNetworkMessage(m_outgoing_messages.back());
}

void NetworkCommunicator::HandleWrite(const boost::system::error_code& e)
{
	if (!e) {
		std::scoped_lock slock(message_send_mutex);
		if (!m_outgoing_messages.empty()) {
			m_outgoing_messages.pop();
		}
	}

	if (e) {
		std::cout << e.what() << std::endl;
	}
}

void NetworkCommunicator::HandleReconnect(const boost::system::error_code& e)
{
	if (e)
	{
		m_reconnect_timer.expires_after(boost::asio::chrono::seconds(1));
		m_reconnect_timer.async_wait(
			[this](const boost::system::error_code& e)
			{
				if (!e)
				{
					Connect();
				}
			});
	}
}