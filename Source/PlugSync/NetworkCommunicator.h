#ifndef M1_PANNER_NETWORK_COMMUNICATOR_H
#define M1_PANNER_NETWORK_COMMUNICATOR_H

#include <queue>
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

	bool ReadyToSend();
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
	void HandleReconnect(const boost::system::error_code& e);

	boost::asio::io_context m_io_ctx;
	GA::ConnectionHandlerClient m_connection_handler;
	boost::asio::ip::tcp::resolver::iterator m_endpoint_iterator;

	boost::shared_ptr<GA::Message> m_incoming_message;

	std::queue<boost::shared_ptr<GA::Message>> m_outgoing_messages;
	std::recursive_mutex message_send_mutex;
	std::thread m_message_sender_thread;

	std::string m_session_id;
	int m_instance_index;

	// It could take some time for the networking to get fully connected
	// and nothing currently stops the user from sending messages prior to that
	bool m_ready_to_send = false;

	// Used in case we lose connection
	boost::asio::steady_timer m_reconnect_timer;

};

#endif // M1_PANNER_NETWORK_COMMUNICATOR_H
