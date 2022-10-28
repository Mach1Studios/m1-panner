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

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void parameterChanged(const juce::String &parameterID, float newValue) override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
#endif
    // For Mach1Spatial 8 only (to deal with ProTools 7.1 channel order)
    std::vector<juce::AudioChannelSet::ChannelType> orderOfChans;
    std::vector<int> chanIndexs;
    void fillChannelOrderArray(int numOutputChannels);
    
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
#ifdef STREAMING_PANNER_PLUGIN
    static juce::String paramInputMode;
    static juce::String paramOutputMode;
#endif

#ifdef ITD_PARAMETERS
    // Delay init
    static juce::String paramITDActive;
    static juce::String paramDelayTime;
    static juce::String paramITDClampActive;
    static juce::String paramDelayDistance;
    
    int mSliderDelayTime;
#endif
        
    // Variables from processor for UI
    juce::Array<float> outputMeterValuedB;
    
    Mach1Encode m1Encode;
    PannerSettings pannerSettings;
    MixerSettings monitorSettings;
    juce::PluginHostType hostType;
    
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
