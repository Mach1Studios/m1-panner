#ifndef PANNEROSC_H_INCLUDED
#define PANNEROSC_H_INCLUDED

#include <JuceHeader.h>

class PannerOSC : private juce::OSCSender, private juce::OSCReceiver, private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>

{
    virtual bool init(int serverPort) = 0;
    bool initFromSettings(std::string jsonSettingsFilePath);
	int serverPort, port;
	bool isConnected = false;
	std::function<void(juce::OSCMessage msg)> messageReceived;
	void oscMessageReceived(const juce::OSCMessage& msg) override;

public:
	PannerOSC();
	~PannerOSC();

	void update();
	void AddListener(std::function<void(juce::OSCMessage msg)> messageReceived);
	bool Send(const juce::OSCMessage& msg);
	bool IsConnected();
};


#endif  
