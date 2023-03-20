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

/*#include <message_protocol/Control/EncodeSettingsMessage.h>

#include <message_protocol/Control/ExternalMixerSettings.h>
#include <message_protocol/Control/AudioTimelineUpdateMessage.h>
*/

//BOOST_CLASS_EXPORT(AudioTimelineUpdateMessage)

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
	: m_mixer_handler(owner), m_connection_handler(m_io_ctx.get_executor())
{
	ip::tcp::resolver resolver(m_io_ctx);
	m_endpoint_iterator = resolver.resolve(ip::tcp::resolver::query(host, service));
}

NetworkCommunicator::~NetworkCommunicator()
{
}

void NetworkCommunicator::Connect()
{
	m_connection_handler.GetSocket().async_connect(m_endpoint_iterator->endpoint(),
		[this](const boost::system::error_code& e)
		{
			OnSuccessfulConnection(e);
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

void NetworkCommunicator::SetInstanceInfo(std::string session, int instance_index)
{
	m_session_id = session;
	m_instance_index = instance_index;
}

void NetworkCommunicator::UpdateTrackInfo(std::string track_name, std::string colour)
{
	PannerTrackNameParameters params{ m_instance_index, track_name, colour };
	m_current_message = boost::make_shared<PannerTrackNameMessage>(params);
	SendNetworkMessage(m_current_message);
}

void NetworkCommunicator::OnSuccessfulConnection(const boost::system::error_code& e)
{
	// When we've successfully connected, we send our instance index and session ID
	// We then wait to read for an Ack response, telling us that the external mixer has initialized its data
	if (!e)
	{
		auto init_data = ConnectionRequestParameters{ m_session_id, m_instance_index };
		m_current_message = boost::make_shared<ConnectionRequestMessage>(init_data);
		m_connection_handler.async_write(m_current_message, boost::bind(
			&NetworkCommunicator::DoRead,
			this,
			boost::asio::placeholders::error)
		);
	}
	else {
		std::cout << e.message() << std::endl;
	}
}

void NetworkCommunicator::DoRead(const boost::system::error_code& e)
{
	if (!e) {
		m_connection_handler.async_read(m_current_message,
			boost::bind(
				&NetworkCommunicator::HandleRead,
				this,
				boost::asio::placeholders::error));
	}
}

void NetworkCommunicator::HandleIncomingMessage(const boost::shared_ptr<GA::Message>& message)
{
	switch (message->GetMessageId())
	{
	case GA::Ack: {
		// We've been acknowledged, respond with the current instance Panner Settings
		std::cout << "Received Ack, responding with settings..." << std::endl;
		//TODO Helper method to be called when proc/ui updates param
		// TODO: Retrieve the current state of the parameters here...

		PannerSettingsParameters settings{};
		auto& curr_set = m_mixer_handler.GetOwner().pannerSettings;
		//TODO: Extract this in a class which can easily adapt between two msgs

		ConvertPannerSettings(curr_set, settings);
		m_current_message = boost::make_shared<GA::PannerSettingsMessage>(settings);
		SendNetworkMessage(m_current_message);

		break;
	}
	}
}

void NetworkCommunicator::HandleRead(const boost::system::error_code& e)
{
	if (!e) {
		HandleIncomingMessage(m_current_message);
	}
	else {
		std::cout << e.message() << std::endl;
	}

	m_connection_handler.async_read(m_current_message,
		boost::bind(&NetworkCommunicator::HandleReadTimeout,
			this,
			boost::asio::placeholders::error));
}

void NetworkCommunicator::HandleReadTimeout(const boost::system::error_code& e)
{
	if (!e) {
		m_connection_handler.async_read(m_current_message,
			boost::bind(&NetworkCommunicator::HandleRead,
				this,
				boost::asio::placeholders::error));
	}
	else {
		std::cout << e.message() << std::endl;
	}
}

void NetworkCommunicator::SendNetworkMessage(const boost::shared_ptr<GA::Message>& message)
{
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
	m_current_message = boost::make_shared<PannerSettingsMessage>(parameters, is_update_message);

	SendNetworkMessage(m_current_message);
}

void NetworkCommunicator::UpdateTooltip(std::string message, int timeout)
{
	TooltipUpdateParameters data{ message, timeout };
	m_current_message = boost::make_shared<TooltipUpdateMessage>(data);
	SendNetworkMessage(m_current_message);
}

void NetworkCommunicator::HandleWrite(const boost::system::error_code& e)
{
	if (e)
	{
		std::cout << e.what() << std::endl;
	}
}