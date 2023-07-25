#include "PannerOSC.h"

void PannerOSC::oscMessageReceived(const juce::OSCMessage& msg)
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
		if (socket.bindToPort(port))
		{
			socket.shutdown();
            juce::OSCReceiver::connect(port);
			break;
		}
	}
    juce::OSCReceiver::addListener(this);
}

void PannerOSC::update()
{
	if (!isConnected)
	{
		if (juce::OSCSender::connect("127.0.0.1", 6345))
		{
            juce::OSCMessage msg("/panner/port");
			msg.addInt32(port);
			isConnected = juce::OSCSender::send(msg);
		}
	}
}

void PannerOSC::AddListener(std::function<void(juce::OSCMessage msg)> messageReceived)
{
	this->messageReceived = messageReceived;
}

PannerOSC::~PannerOSC()
{
    juce::OSCSender::disconnect();
    juce::OSCReceiver::disconnect();
}

bool PannerOSC::Send(const juce::OSCMessage& msg)
{
	return (isConnected && juce::OSCSender::send(msg));
}

bool PannerOSC::IsConnected()
{
	return isConnected;
}

