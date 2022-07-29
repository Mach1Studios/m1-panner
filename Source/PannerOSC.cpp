
#include "PannerOSC.h"

void PannerOSC::oscMessageReceived(const OSCMessage& msg)
{
	if (messageReceived != nullptr)
	{
		messageReceived(msg);
	}
}

PannerOSC::PannerOSC()
{
	isConnected = false;
	 
	// check port
	juce::DatagramSocket socket(false);
	socket.setEnablePortReuse(false);

	// find available port
	for (port = 10001; port < 10100; port++)
	{
		//if (OSCReceiver::connect(port)) break;
		if (socket.bindToPort(port))
		{
			socket.shutdown();
			OSCReceiver::connect(port);
			break;
		}
	}

	OSCReceiver::addListener(this);
}

void PannerOSC::update()
{
	if (!isConnected)
	{
		if (OSCSender::connect("127.0.0.1", 10000))
		{
			OSCMessage msg("/panner/port");
			msg.addInt32(port);
			isConnected = OSCSender::send(msg);
		}
	}
}

void PannerOSC::AddListener(std::function<void(OSCMessage msg)> messageReceived)
{
	this->messageReceived = messageReceived;
}

PannerOSC::~PannerOSC()
{
	OSCSender::disconnect();
	OSCReceiver::disconnect();
}

bool PannerOSC::Send(const OSCMessage& msg)
{
	return (isConnected && OSCSender::send(msg));
}

bool PannerOSC::IsConnected()
{
	return isConnected;
}

