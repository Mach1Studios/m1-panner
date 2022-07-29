#ifndef PANNEROSC_H_INCLUDED
#define PANNEROSC_H_INCLUDED

#include "../JuceLibraryCode/JuceHeader.h"

class PannerOSC :
	private OSCSender,
	private OSCReceiver,
	private OSCReceiver::Listener<OSCReceiver::MessageLoopCallback>

{
	int port;
	bool isConnected = false;

	std::function<void(OSCMessage msg)> messageReceived;

	void oscMessageReceived(const OSCMessage& msg) override;

public:
	PannerOSC();

	void update();

	void AddListener(std::function<void(OSCMessage msg)> messageReceived);

	~PannerOSC();

	bool Send(const OSCMessage& msg);

	bool IsConnected();
};


#endif  
