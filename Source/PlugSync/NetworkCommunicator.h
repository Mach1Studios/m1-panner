#ifndef M1_PANNER_NETWORK_COMMUNICATOR_H
#define M1_PANNER_NETWORK_COMMUNICATOR_H

#include <boost/asio.hpp>

#include "shared/message_protocol_external_mixer/include/connection_handler/ConnectionHandlerClient.h"
#include "shared/message_protocol_external_mixer/include/message_protocol/Message.h"

struct PannerSettings;
class ExternalMixerHandler;

/**
 * Responsible for the communication in both ways when an active connection has been
 */
class NetworkCommunicator
{
public:
	/**
	 *
	 */
	NetworkCommunicator(ExternalMixerHandler& owner, const std::string& host, const std::string& service);
	~NetworkCommunicator();
	// Extract into an interface

	void Connect();
	void Start();
	void Stop();
	// Sets the PlugSync related data

	void SetInstanceInfo(std::string session, int instance_index);

	// Sets the JUCE track name & color data
	void UpdateTrackInfo(std::string track_name, std::string colour);
	void SendNetworkMessage(const boost::shared_ptr<GA::Message>& message);
	void UpdateInstance(PannerSettings& settings);
	void UpdateTooltip(std::string, int timeout);
private:
	ExternalMixerHandler& m_mixer_handler;

	void OnSuccessfulConnection(const boost::system::error_code& e);
	void DoRead(const boost::system::error_code& e);
	void HandleIncomingMessage(const boost::shared_ptr<GA::Message>& message);
	void HandleRead(const boost::system::error_code& e);
	void HandleReadTimeout(const boost::system::error_code& e);
	void HandleWrite(const boost::system::error_code& e);

	boost::asio::io_context m_io_ctx;
	GA::ConnectionHandlerClient m_connection_handler;
	boost::asio::ip::tcp::resolver::iterator m_endpoint_iterator;

	boost::shared_ptr<GA::Message> m_current_message;

	std::string m_session_id;
	int m_instance_index;
};

#endif // M1_PANNER_NETWORK_COMMUNICATOR_H
