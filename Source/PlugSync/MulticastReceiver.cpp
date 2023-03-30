#include "MulticastReceiver.h"

#include "ExternalMixerHandler.h"

#include <boost/bind/bind.hpp>

#include <utility>

#include <boost/json.hpp>

using namespace GA;
using namespace boost::system;
using namespace boost::asio;

void EnableDebugWindow() {
#ifdef _WIN32
    // TODO: REMOVE THIS ASAP!
#    define DEBUG
#    ifdef DEBUG

    AllocConsole();
    freopen("CONOUT$", "w", stdout);
    freopen("CONOUT$", "w", stderr);

    std::cout << "Debug keyboard shortcuts can be used." << std::endl;

#    endif
#endif
}

MulticastReceiver::MulticastReceiver(ExternalMixerHandler& external_mixer, std::string listening_address,
	std::string multicast_address, short multicast_port) : m_external_mixer(external_mixer), m_socket_descriptor(m_io_context),
	m_listening_ip_address(std::move(listening_address)),
	m_multicast_address(std::move(multicast_address)),
	m_multicast_port(multicast_port) {
	m_listening_endpoint = ip::udp::endpoint(ip::make_address(m_listening_ip_address), m_multicast_port);

	EnableDebugWindow();

	// Open the socket for listening
	m_socket_descriptor.open(m_listening_endpoint.protocol());

	// set socket options, INADDR_ANY
	m_socket_descriptor.set_option(ip::udp::socket::reuse_address(true));

	m_socket_descriptor.bind(m_listening_endpoint);

	printf("Listening on: %s\n", m_listening_endpoint.address().to_string().c_str());

	// Join the multicast group for listening
	m_socket_descriptor.set_option(ip::multicast::join_group(ip::make_address(m_multicast_address)));

	// Start
	m_socket_descriptor.async_receive_from(
		buffer(m_received_data, max_message_length),
		m_sender_endpoint,
		[&](const boost::system::error_code e, size_t bytes_transferred)
		{
			ReceiveFrom(e, bytes_transferred);
		});
}

void MulticastReceiver::ReceiveFrom(const error_code& error, size_t bytes_received) {
	if (error) {
		// Something 
		printf("\n%s\n", error.what().c_str());
		return;
	}

	{
		std::scoped_lock slock(m_rw_mtx);
		printf("Received %s from %zu bytes\n", m_received_data.data(), bytes_received);
		m_last_message = std::string(m_received_data.data());
	}

	// TODO: Make these shared constants
	// ip
	// port

	try {
		const auto parsed_message = boost::json::parse(m_last_message);

		const auto ip = parsed_message.at("ip").as_string().c_str();
		const auto port = parsed_message.at("port").as_int64();

		m_external_mixer.InitiateConnection(ip, std::to_string(port));
	}
	catch (...) {
		try {
			std::rethrow_exception(std::current_exception());
		}
		catch (const std::exception& e)
		{
			std::cout << e.what() << std::endl;
		}
	}

	// TODO: Its possible that we really just need a single message to be received.
	//
	/*m_socket_descriptor.async_receive_from(
		boost::asio::buffer(m_received_data, max_message_length),
		m_sender_endpoint,
		[&](const boost::system::error_code e, size_t bytes_transferred) {
			ReceiveFrom(e, bytes_transferred);
		});*/
}

std::string MulticastReceiver::GetLastMessage() {
	std::scoped_lock slock(m_rw_mtx);
	return m_last_message;
}

void MulticastReceiver::StartListening()
{
	m_io_context.run();
}

void MulticastReceiver::StopListening()
{
	m_io_context.stop();
}