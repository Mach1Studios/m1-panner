#pragma once

#include <JuceHeader.h>

class PannerOSC : private juce::OSCSender, private juce::OSCReceiver, private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
    bool init(int helperPort);
    bool initFromSettings(std::string jsonSettingsFilePath);
	int helperPort = 0, port = 0;
	bool isConnected = false;
	std::function<void(juce::OSCMessage msg)> messageReceived;
	void oscMessageReceived(const juce::OSCMessage& msg) override;
    juce::uint32 lastMessageTime = 0; 

public:
	PannerOSC();
	~PannerOSC();

	void update();
	void AddListener(std::function<void(juce::OSCMessage msg)> messageReceived);
	bool Send(const juce::OSCMessage& msg);
	bool IsConnected();
    bool sendPannerSettings(int input_mode, float azimuth, float elevation, float diverge, float gain);
};
