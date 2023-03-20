#include "ExternalMixerHandler.h"

#include "MulticastReceiver.h"

#include "NetworkCommunicator.h"

// TODO: this is a nasty way to include plugin-side message into our external mixer handler
// We should have some kind of a better way to convert messages; This will do for Phase 1
#include "../TypesForDataExchange.h"

constexpr auto MULTICAST_TRANSMIT_ADDRESS = "239.255.0.1";
constexpr auto MULTICAST_TRANSMIT_PORT = 790;

ExternalMixerHandler::ExternalMixerHandler(M1PannerAudioProcessor& owner) :
	m_owner(owner),
	m_multicast_receiver(std::make_unique<GA::MulticastReceiver>(*this, "0.0.0.0",
		MULTICAST_TRANSMIT_ADDRESS, MULTICAST_TRANSMIT_PORT)),
	m_plug_sync(std::make_unique<PluginInstanceSynchronizer>()),
	m_current_plugin_index(m_plug_sync->GetPluginInstantiationIndex())
{
}

ExternalMixerHandler::~ExternalMixerHandler()
{
	m_multicast_receiver->StopListening();
	m_multicast_listening_thread.join();
}

PluginInstanceSynchronizer& ExternalMixerHandler::GetPlugSync() const
{
	return *m_plug_sync;
}

size_t ExternalMixerHandler::GetCurrentPluginIndex() const
{
	return m_current_plugin_index;
}

std::string ExternalMixerHandler::GetPluginSessionIdentifier() const
{
	return m_plug_sync->GetPluginSessionId();
}

void ExternalMixerHandler::InitiateConnection(const std::string& network_address, const std::string& port)
{
	m_network_communicator = std::make_unique<NetworkCommunicator>(*this, network_address, port);
	m_network_communicator->SetInstanceInfo(GetPluginSessionIdentifier(), GetCurrentPluginIndex());
	m_network_communicator->Connect();
	m_network_communicator->Start();
}

bool ExternalMixerHandler::IsExternalMixerAvailable()
{
	return m_multicast_receiver->GetLastMessage().empty() ? false : true;
}

void ExternalMixerHandler::StartListeningForExternalMixer()
{
	m_multicast_listening_thread = std::thread([&] {
		m_multicast_receiver->StartListening();
		});
}

M1PannerAudioProcessor& ExternalMixerHandler::GetOwner() const
{
	return m_owner;
}

void ExternalMixerHandler::UpdateTrackInfo(std::string track_name, std::string track_color)
{
	m_network_communicator->UpdateTrackInfo(track_name, track_color);
}

void ExternalMixerHandler::UpdateInstanceInfo(PannerSettings& new_settings)
{
	m_network_communicator->UpdateInstance(new_settings);
}

void ExternalMixerHandler::UpdateTooltip(std::string message, size_t timeout_ms)
{
	m_network_communicator->UpdateTooltip(message, timeout_ms);
}

void ExternalMixerHandler::StopNetworkHandler()
{
	m_network_communicator->Stop();
}
