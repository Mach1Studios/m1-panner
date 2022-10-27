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

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

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
        
    // Variables from processor for UI
    juce::Array<float> outputMeterValuedB;
    
    Mach1Encode m1Encode;
    PannerSettings pannerSettings;
    MixerSettings monitorSettings;
    juce::PluginHostType hostType;
    
private:
    void CreateLayout();
    
    juce::UndoManager mUndoManager;
    juce::AudioProcessorValueTreeState parameters;
    
    // Channel input
    std::vector<std::vector<float>> audioDataIn;
    std::vector<std::vector<juce::LinearSmoothedValue<float>>> smoothedChannelCoeffs;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (M1PannerAudioProcessor)
};
