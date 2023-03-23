/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include <Mach1Encode.h>
#include "Config.h"
#include "TypesForDataExchange.h"
//#include "LegacyParameters.h"

#ifdef ITD_PARAMETERS
#include "RingBuffer.h"
#endif

//==============================================================================
/**
*/
class M1PannerAudioProcessor  : public juce::AudioProcessor, public juce::AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    M1PannerAudioProcessor();
    ~M1PannerAudioProcessor() override;
    
    static AudioProcessor::BusesProperties getHostSpecificLayout() {
        // This determines the initial bus i/o for plugin on construction and depends on the `isBusesLayoutSupported()`
        juce::PluginHostType hostType;
        
        // Multichannel Pro Tools
        // TODO: Check if Ultimate/HD
        if (hostType.isProTools()) {
            return BusesProperties()
                .withInput("Default Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Default Output", juce::AudioChannelSet::create7point1(), true);
        }
        
        // Multichannel DAWs
        if (hostType.isReaper() || hostType.isNuendo() || hostType.isDaVinciResolve() || hostType.isArdour()) {
            if (hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_VST3) {
                return BusesProperties()
                // VST3 requires named plugin configurations only
                .withInput("Input", juce::AudioChannelSet::namedChannelSet(6), true)
                .withOutput("Mach1 Out", juce::AudioChannelSet::ambisonic(5), true); // 36 named channel
            } else {
                return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::namedChannelSet(6), true)
                .withOutput("Mach1 Out", juce::AudioChannelSet::discreteChannels(60), true);
            }
        }
        
        if (hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_Standalone ||
            hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_Unity) {
            return BusesProperties()
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Mach1 Out", juce::AudioChannelSet::discreteChannels(8), true);
        }
        
        // STREAMING Panner instance
        return BusesProperties()
            .withInput("Input", juce::AudioChannelSet::stereo(), true)
            .withOutput("Output", juce::AudioChannelSet::stereo(), true);
    }
    
    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void parameterChanged(const juce::String &parameterID, float newValue) override;
    std::vector<juce::AudioChannelSet::ChannelType> orderOfChans; // For Mach1Spatial 8 only (to deal with ProTools 7.1 channel order)
    std::vector<int> output_channel_indices;
    void fillChannelOrderArray(int numOutputChannels);

#ifndef CUSTOM_CHANNEL_LAYOUT
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif
    
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
#ifdef ITD_PARAMETERS
    void writeToDelayBuffer (juce::AudioSampleBuffer& buffer,
                             const int channelIn, const int channelOut,
                             const int writePos,
                             float startGain, float endGain,
                             bool replacing);
    
    void readFromDelayBuffer (juce::AudioSampleBuffer& buffer,
                              const int channelIn, const int channelOut,
                              const int readPos,
                              float startGain, float endGain,
                              bool replacing);
#endif

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Parameter Setup
    juce::AudioProcessorValueTreeState& getValueTreeState();
    static juce::String paramAzimuth;
    static juce::String paramElevation; // also Z
    static juce::String paramDiverge;
    static juce::String paramGain;
    static juce::String paramX;
    static juce::String paramY;
    static juce::String paramStereoOrbitAzimuth;
    static juce::String paramStereoSpread;
    static juce::String paramStereoInputBalance;
    static juce::String paramAutoOrbit;
    static juce::String paramIsotropicEncodeMode;
    static juce::String paramEqualPowerEncodeMode;
#ifndef CUSTOM_CHANNEL_LAYOUT
    static juce::String paramInputMode;
    static juce::String paramOutputMode;
#endif

#ifdef ITD_PARAMETERS
    // Delay init
    static juce::String paramITDActive;
    static juce::String paramDelayTime;
    static juce::String paramDelayDistance;
    int mSliderDelayTime;
#endif
        
    // Variables from processor for UI
    juce::Array<float> outputMeterValuedB;
    
    double processorSampleRate = 44100; // only has to be something for the initilizer to work
    void m1EncodeChangeInputMode(Mach1EncodeInputModeType inputMode);
    void m1EncodeChangeOutputMode(Mach1EncodeOutputModeType outputMode);
    PannerSettings pannerSettings;
    MixerSettings monitorSettings;
    HostTimelineData hostTimelineData;
    juce::PluginHostType hostType;
    bool layoutCreated = false;
    
    // TODO: change this
    bool external_spatialmixer_active = false; // global detect spatialmixer
    
private:
    void createLayout();
    
    juce::UndoManager mUndoManager;
    juce::AudioProcessorValueTreeState parameters;
    
    // Channel input
    std::vector<std::vector<float>> audioDataIn;
    std::vector<std::vector<juce::LinearSmoothedValue<float>>> smoothedChannelCoeffs;
    
#ifdef ITD_PARAMETERS
    inline void processBuffers (AudioSampleBuffer& buffer,
                        std::vector<int> orderChans, std::vector<std::vector<float> > delayCoeffs);
#else
    inline void processBuffers (juce::AudioSampleBuffer& buffer,
                        std::vector<int> orderChans);
#endif
    
#ifdef ITD_PARAMETERS
    // Delay init
    juce::AudioSampleBuffer mDelayBuffer;
    std::unique_ptr<RingBuffer> ring;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mDelayTimeSmoother; // smoothing for parameters
    float mLastInputGain    = 0.0f;
    int    mWritePos        = 0;
    int    mExpectedReadPos = -1;
    double mSampleRate      = 0;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (M1PannerAudioProcessor)
};
