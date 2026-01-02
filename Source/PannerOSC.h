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
    bool sendRequestForCurrentChannelConfig();
    bool sendPannerSettings(int state);
    bool sendPannerSettings(int state, std::string displayName, juce::OSCColour colour, int input_mode, float azimuth, float elevation, float diverge, float gain, int panner_mode, bool gain_comp_active, bool st_auto_orbit, float st_azimuth, float st_spread);

    // Streaming mode OSC methods
    /**
     * Register this panner as a streaming source with m1-system-helper
     * @param sessionId Host process ID (groups panners from same DAW session)
     * @param pannerUuid Unique identifier for this panner instance
     * @param mmapPath Path to the memory-mapped file containing audio stream
     * @param numChannels Number of Mach1 output channels (4, 8, or 14)
     * @param sampleRate Audio sample rate
     * @param maxBlockSize Maximum expected block size
     * @return true if registration message sent successfully
     */
    bool registerStreamingPanner(const juce::String& sessionId,
                                 const juce::String& pannerUuid,
                                 const juce::String& mmapPath,
                                 int numChannels,
                                 int sampleRate,
                                 int maxBlockSize);
    
    /**
     * Unregister this panner from streaming mode
     * @param pannerUuid Unique identifier for this panner instance
     * @return true if unregistration message sent successfully
     */
    bool unregisterStreamingPanner(const juce::String& pannerUuid);
    
    /**
     * Send streaming heartbeat to keep registration alive
     * Called from timer callback (message thread, not audio thread)
     * @param pannerUuid Unique identifier for this panner instance
     * @return true if heartbeat sent successfully
     */
    bool sendStreamingHeartbeat(const juce::String& pannerUuid);
    
    /**
     * Check if streaming is acknowledged by helper
     */
    bool isStreamingAcknowledged() const { return streamingAcknowledged; }

private:
    bool is_connected = false; // used to track connection with helper utility
    bool streamingAcknowledged = false; // helper confirmed streaming registration
};
