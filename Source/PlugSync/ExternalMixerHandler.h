#ifndef M1_PANNER_EXTERNALMIXERHANDLER_H
#define M1_PANNER_EXTERNALMIXERHANDLER_H

#include "PluginInstanceSynchronizer.h"

#include <memory>
#include <boost/smart_ptr/shared_ptr.hpp>

// Due to boost complaining about winsock being already defined
// We'll just forward declare and use
namespace GA
{
	class Message;
	class MulticastReceiver;
}

class NetworkCommunicator;
class M1PannerAudioProcessor;
class PannerSettings;

/**
 * Class used for integration with the Mach1 External Mixer
 * It is currently responsible for Plugin instance information, bidirectional communication
 */
class ExternalMixerHandler
{
public:
	ExternalMixerHandler(M1PannerAudioProcessor& owner);

	~ExternalMixerHandler();

	[[nodiscard]] PluginInstanceSynchronizer& GetPlugSync() const;

	[[nodiscard]] size_t GetCurrentPluginIndex() const;

	[[nodiscard]] std::string GetPluginSessionIdentifier() const;

	void InitiateConnection(const std::string& network_address, const std::string& port);

	bool IsExternalMixerAvailable() const;

	void StartListeningForExternalMixer();

	[[nodiscard]] M1PannerAudioProcessor& GetOwner() const;

	// PluginControl Interface; todo extract

	// Wrapper around NetworkCommunicator's SendNetworkMessage
	void UpdateTrackInfo(std::string track_name, std::string track_color);

	void UpdateInstanceInfo(PannerSettings& new_settings);

	void UpdateTooltip(std::string message, size_t timeout_ms = 3000);

private:
	M1PannerAudioProcessor& m_owner;

	std::unique_ptr<GA::MulticastReceiver> m_multicast_receiver;
	std::unique_ptr<NetworkCommunicator>m_network_communicator;
	// External Mixer Identification
	std::unique_ptr<PluginInstanceSynchronizer> m_plug_sync;
	size_t m_current_plugin_index;
	//std::string m_plugin_session_identifier;
	std::thread m_multicast_listening_thread;
	std::thread m_network_communicator_thread;
};

#endif //M1_PANNER_EXTERNALMIXERHANDLER_H
