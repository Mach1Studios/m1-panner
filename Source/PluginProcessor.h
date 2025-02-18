/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include "Config.h"
#include "AlertData.h"
#include "PannerOSC.h"
#include "TypesForDataExchange.h"
#include <JuceHeader.h>
#include <Mach1Encode.h>
#include "LegacyParameters.h"

#ifdef ITD_PARAMETERS
    #include "RingBuffer.h"
#endif

//==============================================================================
/**
*/
class PannerOSC; // forward declare for PannerOSC
class M1PannerAudioProcessor : public juce::AudioProcessor, juce::AudioProcessorValueTreeState::Listener, juce::Timer
{
public:
    //==============================================================================
    M1PannerAudioProcessor();
    ~M1PannerAudioProcessor() override;

    static AudioProcessor::BusesProperties getHostSpecificLayout()
    {
        // This determines the initial bus i/o for plugin on construction and depends on the `isBusesLayoutSupported()`
        juce::PluginHostType hostType;

        // Pro Tools specific layout
        if (hostType.isProTools() || hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_AAX)
        {
            // Pro Tools needs a fixed, stable initial configuration
            return BusesProperties()
                .withInput("Default Input", juce::AudioChannelSet::stereo(), true)
                .withOutput("Default Output", juce::AudioChannelSet::create7point1(), true);
        }

        // Multichannel DAWs
        if (hostType.isReaper() || hostType.isNuendo() || hostType.isDaVinciResolve() || hostType.isArdour())
        {
            if (hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_VST3)
            {
                return BusesProperties()
                    // VST3 requires named plugin configurations only
                    .withInput("Input", juce::AudioChannelSet::namedChannelSet(6), true)
                    .withOutput("Mach1 Out", juce::AudioChannelSet::ambisonic(5), true); // 36 named channel
            }
            else
            {
                return BusesProperties()
                    .withInput("Input", juce::AudioChannelSet::namedChannelSet(6), true)
                    .withOutput("Mach1 Out", juce::AudioChannelSet::discreteChannels(60), true);
            }
        }

        if (hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_Unity)
        {
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
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    std::vector<int> output_channel_indices; // For reordering channel indices based on specific DAW hosts (example: reordering for ProTools 7.1 channel order)
    void fillChannelOrderArray(int numM1OutputChannels);

#ifndef CUSTOM_CHANNEL_LAYOUT
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
#ifdef ITD_PARAMETERS
    void writeToDelayBuffer(juce::AudioSampleBuffer& buffer,
        const int channelIn,
        const int channelOut,
        const int writePos,
        float startGain,
        float endGain,
        bool replacing);

    void readFromDelayBuffer(juce::AudioSampleBuffer& buffer,
        const int channelIn,
        const int channelOut,
        const int readPos,
        float startGain,
        float endGain,
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
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // Parameter Setup
    juce::AudioProcessorValueTreeState& getValueTreeState();
    static juce::String paramAzimuth;
    static juce::String paramElevation; // also Z
    static juce::String paramDiverge;
    static juce::String paramGain;
    static juce::String paramStereoOrbitAzimuth;
    static juce::String paramStereoSpread;
    static juce::String paramStereoInputBalance;
    static juce::String paramAutoOrbit;
    static juce::String paramIsotropicEncodeMode;
    static juce::String paramEqualPowerEncodeMode;
    static juce::String paramGainCompensationMode;
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
    void m1EncodeChangeInputOutputMode(Mach1EncodeInputMode inputMode, Mach1EncodeOutputMode outputMode);
    PannerSettings pannerSettings;
    float gain_comp_in_db = 0;
    MixerSettings monitorSettings;
    HostTimelineData hostTimelineData;
    juce::PluginHostType hostType;
    void updateTrackProperties(const TrackProperties& properties) override { track_properties = properties; }
    TrackProperties getTrackProperties() { return track_properties; }
    bool layoutCreated = false;
    bool lockOutputLayout = false;

    // update m1encode obj points
    bool needToUpdateM1EncodePoints = false;
    void updateM1EncodePoints();

    // Communication to OrientationManager/Monitor and the rest of the M1SpatialSystem
    void timerCallback() override;
    std::unique_ptr<PannerOSC> pannerOSC;
    juce::OSCColour osc_colour = { 0, 0, 0, 255 };

    // TODO: change this
    bool external_spatialmixer_active = false; // global detect spatialmixer

    // UI related utility functions
    struct Line2D
    {
        Line2D(double x, double y, double x2, double y2) : x{ x }, y{ y }, x2{ x2 }, y2{ y2 } {}
        MurkaPoint p() const
        {
            return { x, y };
        }
        MurkaPoint v() const
        {
            return { x2 - x, y2 - y };
        }
        double x, y, x2, y2;
    };

    inline double intersection(const Line2D& a, const Line2D& b)
    {
        const double Precision = std::sqrt(std::numeric_limits<double>::epsilon());
        double d = a.v().x * b.v().y - a.v().y * b.v().x;
        if (std::abs(d) < Precision)
            return std::numeric_limits<double>::quiet_NaN();
        else
        {
            double n = (b.p().x - a.p().x) * b.v().y
                       - (b.p().y - a.p().y) * b.v().x;
            return n / d;
        }
    }

    inline MurkaPoint intersection_point(const Line2D& a, const Line2D& b)
    {
        // Line2D has an operator () (double r) returning p() + r * v()
        return a.p() + a.v() * (intersection(a, b));
    }

    void convertRCtoXYRaw(float r, float d, float& x, float& y);
    void convertXYtoRCRaw(float x, float y, float& r, float& d);

    // Add mute states vector for each input channel
    std::vector<bool> channelMuteStates;

    // This will be set by the UI or editor so we can notify it of alerts
    std::function<void(const Mach1::AlertData&)> postAlertToUI;
    void postAlert(const Mach1::AlertData& alert);
    std::vector<Mach1::AlertData> pendingAlerts;

private:
    TrackProperties track_properties;
    void createLayout();

    juce::UndoManager mUndoManager;
    juce::AudioProcessorValueTreeState parameters;

    // Channel input
    std::vector<std::vector<float>> audioDataIn;
    std::vector<std::vector<juce::LinearSmoothedValue<float>>> smoothedChannelCoeffs;

#ifdef ITD_PARAMETERS
    inline void processBuffers(AudioSampleBuffer& buffer,
        std::vector<int> orderChans,
        std::vector<std::vector<float>> delayCoeffs);
#else
    inline void processBuffers(juce::AudioSampleBuffer& buffer,
        std::vector<int> orderChans);
#endif

#ifdef ITD_PARAMETERS
    // Delay init
    juce::AudioSampleBuffer mDelayBuffer;
    std::unique_ptr<RingBuffer> ring;
    juce::SmoothedValue<float, juce::ValueSmoothingTypes::Linear> mDelayTimeSmoother; // smoothing for parameters
    float mLastInputGain = 0.0f;
    int mWritePos = 0;
    int mExpectedReadPos = -1;
    double mSampleRate = 0;
#endif

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(M1PannerAudioProcessor)
};
