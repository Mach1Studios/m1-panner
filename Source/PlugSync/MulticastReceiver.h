#ifndef MACH1_EXTERNAL_MIXER_MULTICASTREVEIVER_H
#define MACH1_EXTERNAL_MIXER_MULTICASTREVEIVER_H

#include <boost/asio.hpp>
#include <mutex>

class ExternalMixerHandler;

namespace GA {
	/**
	 * Class used to receive messages from a Multicast sender
	 */
	class MulticastReceiver {
	public:
		MulticastReceiver(ExternalMixerHandler& external_mixer_ref, std::string listening_address, std::string multicast_address, short multicast_port);
		/**
		 * Get the latest released message
		 * @return String containing the message
		 */
		std::string GetLastMessage();

		void StartListening();

		void StopListening();

	private:
		ExternalMixerHandler& m_external_mixer;
		void ReceiveFrom(const boost::system::error_code& error, size_t bytes_received);

		// Object controlling the execution of tasks
		boost::asio::io_context m_io_context;
		boost::asio::ip::udp::socket m_socket_descriptor;
		boost::asio::ip::udp::endpoint m_sender_endpoint;

		/**
		 * This size is valid only for the messages submitted by MulticastSender
		 */
		enum {
			max_message_length = 1024
		};

		std::array<char, max_message_length> m_received_data{};
		std::string m_last_message;
		std::string m_listening_ip_address;
		boost::asio::ip::udp::endpoint m_listening_endpoint;
		std::string m_multicast_address;
		short m_multicast_port;
		std::mutex m_rw_mtx;
	};
}

#endif //MACH1_EXTERNAL_MIXER_MULTICASTREVEIVER_H
