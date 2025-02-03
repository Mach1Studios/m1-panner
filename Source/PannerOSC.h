#pragma once

#include <JuceHeader.h>
#include "AlertData.h"

class M1PannerAudioProcessor;

class PannerOSC : public juce::DeletedAtShutdown, private juce::OSCSender, private juce::OSCReceiver, private juce::OSCReceiver::Listener<juce::OSCReceiver::MessageLoopCallback>
{
public:
    M1PannerAudioProcessor* processor = nullptr;
    PannerOSC(M1PannerAudioProcessor* processor);
    ~PannerOSC();

    bool init(int helperPort);
    bool initFromSettings(const std::string& jsonSettingsFilePath);
    int helperPort = 0, port = 0;
    std::function<void(juce::OSCMessage msg)> messageReceived;
    void oscMessageReceived(const juce::OSCMessage& msg) override;
    juce::uint32 lastMessageTime = 0;

    void update();
    void AddListener(std::function<void(juce::OSCMessage msg)> messageReceived);
    bool Send(const juce::OSCMessage& msg);
    bool isConnected();
    bool sendRequestToChangeChannelConfig(int channel_count_for_config);
    bool sendPannerSettings(int state);
    bool sendPannerSettings(int state, std::string displayName, juce::OSCColour colour, int input_mode, float azimuth, float elevation, float diverge, float gain, int panner_mode, bool st_auto_orbit, float st_azimuth, float st_spread);

private:
    bool is_connected = false; // used to track connection with helper utility
};
