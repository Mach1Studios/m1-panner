/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

// Platform-specific includes for process ID.
#if JUCE_WINDOWS
    #include <windows.h>
#elif JUCE_MAC || JUCE_LINUX || JUCE_IOS || JUCE_ANDROID
    #include <unistd.h>
#endif

namespace
{
bool areUiReticleSnapshotStatesEqual(const M1PannerAudioProcessor::UiReticleSnapshotState& lhs,
                                     const M1PannerAudioProcessor::UiReticleSnapshotState& rhs)
{
    return lhs.azimuth == rhs.azimuth
        && lhs.elevation == rhs.elevation
        && lhs.diverge == rhs.diverge
        && lhs.gain == rhs.gain
        && lhs.stereoOrbitAzimuth == rhs.stereoOrbitAzimuth
        && lhs.stereoSpread == rhs.stereoSpread
        && lhs.autoOrbit == rhs.autoOrbit
        && lhs.isotropicMode == rhs.isotropicMode
        && lhs.equalpowerMode == rhs.equalpowerMode
        && lhs.gainCompensationMode == rhs.gainCompensationMode
        && lhs.monitorMode == rhs.monitorMode
        && lhs.inputMode == rhs.inputMode
        && lhs.outputMode == rhs.outputMode;
}

}
/*
 Architecture:
    - parameterChanged() updates the i/o layout
    - parameterChanged() checks if matched with pannerSettings and otherwise updates this too
    - parameters expect normalized 0->1 except the i/o and pannerSettings which expects unnormalled values
 */

// Static variable definition
bool M1PannerAudioProcessor::s_globalExternalMixerActive = false;

juce::String M1PannerAudioProcessor::paramAzimuth("azimuth");
juce::String M1PannerAudioProcessor::paramElevation("elevation"); // also Z
juce::String M1PannerAudioProcessor::paramDiverge("diverge");
juce::String M1PannerAudioProcessor::paramGain("gain");
juce::String M1PannerAudioProcessor::paramStereoOrbitAzimuth("orbitAzimuth");
juce::String M1PannerAudioProcessor::paramStereoSpread("orbitSpread");
juce::String M1PannerAudioProcessor::paramStereoInputBalance("stereoInputBalance");
juce::String M1PannerAudioProcessor::paramAutoOrbit("autoOrbit");
juce::String M1PannerAudioProcessor::paramIsotropicEncodeMode("isotropicEncodeMode");
juce::String M1PannerAudioProcessor::paramEqualPowerEncodeMode("equalPowerEncodeMode");
juce::String M1PannerAudioProcessor::paramGainCompensationMode("gainCompensationMode");
#ifndef CUSTOM_CHANNEL_LAYOUT
juce::String M1PannerAudioProcessor::paramInputMode("inputMode");
juce::String M1PannerAudioProcessor::paramOutputMode("outputMode");
#endif
#ifdef ITD_PARAMETERS
juce::String M1PannerAudioProcessor::paramITDActive("ITDProcessing");
juce::String M1PannerAudioProcessor::paramDelayTime("DelayTime");
juce::String M1PannerAudioProcessor::paramDelayDistance("ITDDistance");
#endif

//==============================================================================
M1PannerAudioProcessor::M1PannerAudioProcessor()
    : AudioProcessor(getHostSpecificLayout()),
      parameters(*this, &mUndoManager, juce::Identifier("M1-Panner"), {
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramAzimuth, 1), TRANS("Azimuth"), juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), 0.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "°"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramElevation, 1), TRANS("Elevation"), juce::NormalisableRange<float>(-90.0f, 90.0f, 0.01f), 0.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "°"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDiverge, 1), TRANS("Diverge"), juce::NormalisableRange<float>(-100.0f, 100.0f, 0.01f), 50.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1); }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramGain, 1), TRANS("Input Gain"), juce::NormalisableRange<float>(-90.0, 24.0f, 0.1f, std::log(0.5f) / std::log(100.0f / 106.0f)), 0.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + " dB"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramAutoOrbit, 1), TRANS("Auto Orbit"), false),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoOrbitAzimuth, 1), TRANS("Stereo Orbit Azimuth"), juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), 0.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "°"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoSpread, 1), TRANS("Stereo Spread"), juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), 50.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1); }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoInputBalance, 1), TRANS("Stereo Input Balance"), juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), 0.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1); }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramIsotropicEncodeMode, 1), TRANS("Isotropic Encode Mode"), true),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramEqualPowerEncodeMode, 1), TRANS("Equal Power Encode Mode"), true),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramGainCompensationMode, 1), TRANS("Gain Compensation Mode"), true),
#ifndef CUSTOM_CHANNEL_LAYOUT
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID(paramInputMode, 1), TRANS("Input Mode"), 0, Mach1EncodeInputMode::BFOAACN, Mach1EncodeInputMode::Mono),
          // Note: Change init output to max bus size when new formats are introduced
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID(paramOutputMode, 1), TRANS("Output Mode"), 0, Mach1EncodeOutputMode::M1Spatial_14, Mach1EncodeOutputMode::M1Spatial_8),
#endif
#ifdef ITD_PARAMETERS
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramITDActive, 1), TRANS("ITD"), false),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDelayTime, 1), TRANS("Delay Time (max)"), juce::NormalisableRange<float>(0.0f, 10000.0f, 1.0f), 0.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "μS"; }, [](const juce::String& t) { return t.dropLastCharacters(1).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDelayDistance, 1), TRANS("Delay Distance"), juce::NormalisableRange<float>(0.0f, 10000.0f, 0.01f), 0.0f, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + ""; }, [](const juce::String& t) { return t.dropLastCharacters(1).getFloatValue(); }),
#endif
                                                                      })
{
    parameters.addParameterListener(paramAzimuth, this);
    parameters.addParameterListener(paramElevation, this);
    parameters.addParameterListener(paramDiverge, this);
    parameters.addParameterListener(paramGain, this);
    parameters.addParameterListener(paramAutoOrbit, this);
    parameters.addParameterListener(paramStereoOrbitAzimuth, this);
    parameters.addParameterListener(paramStereoSpread, this);
    parameters.addParameterListener(paramStereoInputBalance, this);
    parameters.addParameterListener(paramIsotropicEncodeMode, this);
    parameters.addParameterListener(paramEqualPowerEncodeMode, this);
    parameters.addParameterListener(paramGainCompensationMode, this);
#ifndef CUSTOM_CHANNEL_LAYOUT
    parameters.addParameterListener(paramInputMode, this);
    parameters.addParameterListener(paramOutputMode, this);
#endif
#ifdef ITD_PARAMETERS
    parameters.addParameterListener(paramITDActive, this);
    parameters.addParameterListener(paramDelayTime, this);
    parameters.addParameterListener(paramDelayDistance, this);
#endif

#ifndef CUSTOM_CHANNEL_LAYOUT
    requestedInputMode.store(static_cast<int>(pannerSettings.m1Encode.getInputMode()));
    requestedOutputMode.store(static_cast<int>(pannerSettings.m1Encode.getOutputMode()));
#endif

    // Setup osc and listener
    pannerOSC = std::make_unique<PannerOSC>(this);
    pannerOSC->AddListener([&](juce::OSCMessage msg) {
        if (msg.getAddressPattern() == "/monitor-settings")
        {
            if (msg.size() > 0)
            {
                // Capturing monitor mode
                int mode = msg[0].getInt32();
                monitorSettings.monitor_mode = mode;
            }
            if (msg.size() >= 2)
            {
                // Capturing Monitor's Yaw
                if (msg[1].isFloat32())
                {
                    float yaw = msg[1].getFloat32();
                    monitorSettings.yaw = yaw; // un-normalised
                }
            }
            if (msg.size() >= 3)
            {
                // Capturing Monitor's Pitch
                if (msg[2].isFloat32())
                {
                    float pitch = msg[2].getFloat32();
                    monitorSettings.pitch = pitch; // un-normalized
                    DBG("[OSC] Recieved msg | Mode: " + std::to_string(msg[0].getInt32()) + ", Y: " + std::to_string(msg[1].getFloat32()) + ", P: " + std::to_string(msg[2].getFloat32()));
                }
            }
        }
        else if (msg.getAddressPattern() == "/m1-channel-config")
        {
            DBG("[OSC] Recieved msg | Channel Config: " + std::to_string(msg[0].getInt32()));
            // Capturing monitor active state
            int channel_count = msg[0].getInt32();
            if (!pannerSettings.lockOutputLayout && channel_count != pannerSettings.m1Encode.getInputChannelsCount()) // got a request for a different config
            {
                if (channel_count == 4)
                {
                    parameters.getParameter(paramOutputMode)->setValueNotifyingHost(parameters.getParameter(paramOutputMode)->convertTo0to1(Mach1EncodeOutputMode::M1Spatial_4));
                }
                else if (channel_count == 8)
                {
                    parameters.getParameter(paramOutputMode)->setValueNotifyingHost(parameters.getParameter(paramOutputMode)->convertTo0to1(Mach1EncodeOutputMode::M1Spatial_8));
                }
                else if (channel_count == 14)
                {
                    parameters.getParameter(paramOutputMode)->setValueNotifyingHost(parameters.getParameter(paramOutputMode)->convertTo0to1(Mach1EncodeOutputMode::M1Spatial_14));
                }
                else
                {
                    DBG("[OSC] Error with received channel config!");
                }
            }
        }
    });

    // Get or assign a track color for panner instance -> player
    if (track_properties.colour.has_value() && track_properties.colour->getAlpha() != 0)
    { // unfound colors are 0,0,0,0
        osc_colour.fromInt32(track_properties.colour->getARGB());
    }
    else
    {
        // randomize a color
        osc_colour.red = juce::Random().nextInt(255);
        osc_colour.green = juce::Random().nextInt(255);
        osc_colour.blue = juce::Random().nextInt(255);
        osc_colour.alpha = 255;
    }

    // pannerOSC update timer loop
    startTimer(50);

#if M1_ENABLE_EXTERNAL_RENDERER
    m_uniqueInstanceId = generateUniqueInstanceName();
#endif

    // print build time for debug
    juce::String date(__DATE__);
    juce::String time(__TIME__);
    DBG("[PANNER] Build date: " + date + " | Build time: " + time);
}

M1PannerAudioProcessor::~M1PannerAudioProcessor()
{
    pannerSettings.state.store(-1);
    stopTimer();

#if M1_ENABLE_EXTERNAL_RENDERER
    if (m_memoryShare && m_memoryShareInitialized && m_instanceBaseName.isNotEmpty())
    {
        m_memoryShare.reset();
        M1MemoryShare::deleteSharedMemory(m_instanceBaseName);
        DBG("[M1-Panner] Deleted shared memory file for: " + m_instanceBaseName);
    }
#endif

    Mach1::M1SystemHelperManager::getInstance().releaseHelperService("M1-Panner");
}

//==============================================================================
const juce::String M1PannerAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool M1PannerAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool M1PannerAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool M1PannerAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double M1PannerAudioProcessor::getTailLengthSeconds() const
{
    return 0.0;
}

int M1PannerAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
        // so this should be at least 1, even if you're not really implementing programs.
}

int M1PannerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void M1PannerAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String M1PannerAudioProcessor::getProgramName(int index)
{
    return {};
}

void M1PannerAudioProcessor::changeProgramName(int index, const juce::String& newName)
{
}

//==============================================================================
void M1PannerAudioProcessor::createLayout()
{
    if (!getBus(false, 0) || !getBus(true, 0))
    {
        DBG("Invalid bus configuration in createLayout() - buses not ready yet");
        layoutCreated = false; // Explicitly mark as not created, will retry on next processBlock
        return;
    }

    // Auto-detect external spatial mixer mode for supported configurations
    int inputChannels = getBus(true, 0)->getCurrentLayout().size();
    int outputChannels = getBus(false, 0)->getCurrentLayout().size();

    // Activate external mixer for 1,2 (mono in, stereo out) or 2,2 (stereo in, stereo out) configurations
#if M1_ENABLE_EXTERNAL_RENDERER
    if ((inputChannels == 1 && outputChannels == 2) || (inputChannels == 2 && outputChannels == 2))
    {
        external_spatialmixer_active = true;
        setExternalSpatialMixerActive(true);
        DBG("[PANNER] External spatial mixer activated for " + juce::String(inputChannels) + "," + juce::String(outputChannels) + " configuration");
    }
    else
    {
        external_spatialmixer_active = false;
        DBG("[PANNER] Internal processing mode for " + juce::String(inputChannels) + "," + juce::String(outputChannels) + " configuration");
    }
#else
    DBG("[PANNER] Internal processing mode for " + juce::String(inputChannels) + "," + juce::String(outputChannels) + " configuration (external renderer disabled)");
#endif

    if (external_spatialmixer_active)
    {
        /// EXTERNAL MULTICHANNEL PROCESSING

        // In external mixer mode, respect user's input mode choice
        // but ensure bus layout is compatible (mono/stereo only)
        if (pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputMode::Mono)
        {
            getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::mono());
        }
        else
        {
            // Default to stereo for all other modes (stereo processing)
            getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
        }

        // For external mixer, respect user's output mode choice but output stereo to host
        // (Internal processing uses user's chosen output mode: 4, 8, or 14 channels)
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::stereo()); // But output stereo to host

        // CRITICAL: Initialize coefficients for external mixer mode
        m1EncodeChangeInputOutputMode(pannerSettings.m1Encode.getInputMode(), pannerSettings.m1Encode.getOutputMode());
    }
    else
    {
        /// INTERNAL MULTICHANNEL PROCESSING

        // check if there is a mismatch of the current bus size on PT
        if (hostType.isProTools())
        {
            // update the pannerSettings if there is a mismatch

            // I/O Concept
            // Inputs: The inputs for this plugin are more literal, only allowing the number of channels available by host to dictate the input mode
            // Outputs: The outputs for this plugin allows the m1Encode object to have a higher channel count output mode than what the host allows to support more configurations on channel specific hosts

            /// INPUTS
            if (getBus(true, 0)->getCurrentLayout().size() != pannerSettings.m1Encode.getInputChannelsCount())
            {
                if (getBus(true, 0)->getCurrentLayout().size() == 1)
                {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputMode::Mono);
                }
                else if (getBus(true, 0)->getCurrentLayout().size() == 2)
                {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputMode::Stereo);
                }
                else if (getBus(true, 0)->getCurrentLayout().size() == 3)
                {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputMode::LCR);
                }
                else if (getBus(true, 0)->getCurrentLayout().size() == 4)
                {
                    if ((pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::Quad) && (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::LCRS) && (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::AFormat) && (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::BFOAACN) && (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::BFOAFUMA))
                    {
                        // if we are not one of the 4ch formats in pro tools then force default of QUAD
                        pannerSettings.m1Encode.setInputMode(Mach1EncodeInputMode::Quad);
                    }
                    else
                    {
                        // already set
                    }
                }
                else if (getBus(true, 0)->getCurrentLayout().size() == 5)
                {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputMode::FiveDotZero);
                }
                else if (getBus(true, 0)->getCurrentLayout().size() == 6)
                {
                    if ((pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::FiveDotOneFilm) && (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::FiveDotOneDTS) && (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputMode::FiveDotOneSMTPE))
                    {
                        // if we are not one of the 4ch formats in pro tools then force default of 5.1 film
                        pannerSettings.m1Encode.setInputMode(Mach1EncodeInputMode::FiveDotOneFilm);
                    }
                    else
                    {
                        // already set
                    }
                }
                else
                {
                    // an unsupported format
                }
            }
            // update parameter from start
            parameters.getParameter(paramInputMode)->setValue(parameters.getParameter(paramInputMode)->convertTo0to1(pannerSettings.m1Encode.getInputMode()));

            /// OUTPUTS
            if (getBus(false, 0)->getCurrentLayout().size() != pannerSettings.m1Encode.getOutputChannelsCount())
            {
                if (getBus(false, 0)->getCurrentLayout().size() == 4)
                {
                    pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputMode::M1Spatial_4);
                    gain_comp_in_db = pannerSettings.m1Encode.getGainCompensation(true); // store new gain compensation
                }
                else if (getBus(false, 0)->getCurrentLayout().size() == 8 || getBus(false, 0)->getCurrentLayout().getAmbisonicOrder() == 2)
                {
                    pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputMode::M1Spatial_8);
                    gain_comp_in_db = pannerSettings.m1Encode.getGainCompensation(true); // store new gain compensation
                }
                else if (getBus(false, 0)->getCurrentLayout().size() >= 14 || getBus(false, 0)->getCurrentLayout().getAmbisonicOrder() >= 3)
                {
                    if ((pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputMode::M1Spatial_4) && (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputMode::M1Spatial_8) && (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputMode::M1Spatial_14))
                    {
                        pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputMode::M1Spatial_14);
                        gain_comp_in_db = pannerSettings.m1Encode.getGainCompensation(true); // store new gain compensation
                    }
                }
            }
            // update parameter from start
            parameters.getParameter(paramOutputMode)->setValue(parameters.getParameter(paramOutputMode)->convertTo0to1(pannerSettings.m1Encode.getOutputMode()));
        }
        // apply the i/o to the plugin
        m1EncodeChangeInputOutputMode(pannerSettings.m1Encode.getInputMode(), pannerSettings.m1Encode.getOutputMode());
    }

   #ifndef CUSTOM_CHANNEL_LAYOUT
    requestedInputMode.store(static_cast<int>(pannerSettings.m1Encode.getInputMode()));
    requestedOutputMode.store(static_cast<int>(pannerSettings.m1Encode.getOutputMode()));
   #endif

    layoutCreated.store(true); // flow control for static i/o
    updateHostDisplay();
}

void M1PannerAudioProcessor::applyPendingModeChange()
{
#ifndef CUSTOM_CHANNEL_LAYOUT
    if (!pendingModeChange.exchange(false))
        return;

    const auto requestedInput = static_cast<Mach1EncodeInputMode>(requestedInputMode.load());
    const auto requestedOutput = static_cast<Mach1EncodeOutputMode>(requestedOutputMode.load());

    if (!hostType.isProTools() || (hostType.isProTools() && (getTotalNumInputChannels() == 4 || getTotalNumInputChannels() == 6)))
        pannerSettings.m1Encode.setInputMode(requestedInput);

    if (!hostType.isProTools() || (hostType.isProTools() && getTotalNumOutputChannels() > 8))
        pannerSettings.m1Encode.setOutputMode(requestedOutput);

    createLayout();
    pendingPannerSettingsSend.store(true);
#endif
}

void M1PannerAudioProcessor::applyPendingStereoParameterReset()
{
    if (!pendingStereoParameterReset.exchange(false))
        return;

    if (!pannerSettings.autoOrbit)
    {
        auto* stereoOrbitParam = parameters.getParameter(paramStereoOrbitAzimuth);
        auto* stereoSpreadParam = parameters.getParameter(paramStereoSpread);
        auto* stereoBalanceParam = parameters.getParameter(paramStereoInputBalance);

        stereoOrbitParam->setValueNotifyingHost(stereoOrbitParam->convertTo0to1(0.0f));
        stereoSpreadParam->setValueNotifyingHost(stereoSpreadParam->convertTo0to1(0.0f));
        stereoBalanceParam->setValueNotifyingHost(stereoBalanceParam->convertTo0to1(0.0f));
    }
}

//==============================================================================
void M1PannerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    if (!layoutCreated.load())
    {
        createLayout();
    }

    // can still be used to calculate coeffs even in STREAMING_PANNER_PLUGIN mode
    processorSampleRate = sampleRate;

    if (pannerSettings.m1Encode.getOutputChannelsCount() != getMainBusNumOutputChannels())
    {
        bool channel_io_error = -1;
        // error handling here?
    }

    // Checks if output bus is non DISCRETE layout and fixes host specific channel ordering issues
    fillChannelOrderArray(pannerSettings.m1Encode.getOutputChannelsCount());

#ifdef ITD_PARAMETERS
    mSampleRate = sampleRate;
    mDelayTimeSmoother.reset(samplesPerBlock);
    mDelayTimeSmoother.setValue(0);

    ring.reset(new RingBuffer(pannerSettings..getOutputChannelsCount(), 64 * sampleRate));
    ring->clear();

    // sample buffer for 2 seconds + MAX_NUM_CHANNELS buffers safety
    mDelayBuffer.setSize(pannerSettings..getOutputChannelsCount(), (float)pannerSettings.m1Encode.getOutputChannelsCount() * (samplesPerBlock + sampleRate), false, false);
    mDelayBuffer.clear();
    mExpectedReadPos = -1;
#endif

    // Initialize OSC if not already done
    if (!pannerOSC) {
        pannerOSC = std::make_unique<PannerOSC>(this);
        if (!pannerOSC->init(9001)) {
            Mach1::AlertData alert;
            alert.title = "Initialization Warning";
            alert.message = "Could not initialize network communication. Some features may be limited.";
            alert.buttonText = "OK";
            postAlert(alert);
        }
    }
    
    // Initialize memory sharing if in external spatial mixer mode
    // This ensures the memory file is created even before audio starts playing
#if M1_ENABLE_EXTERNAL_RENDERER
    if (external_spatialmixer_active && !m_memoryShareInitialized)
        initializeMemorySharing();
#endif
}

void M1PannerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
    
    // Reset layout state to ensure clean re-initialization on next prepareToPlay
    layoutCreated = false;
    
    // Clean up memory sharing
#if M1_ENABLE_EXTERNAL_RENDERER
    if (m_memoryShare)
        m_memoryShareInitialized = false;
        // Don't destroy the memory share here - it will be cleaned up in destructor
        // Just mark it as not initialized for safety
#endif
}

void M1PannerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Expects non-normalised values

    needToUpdateM1EncodePoints.store(true); // need to call to update the m1encode obj for new point counts
    uiReticleSnapshotDirty.store(true);

    if (parameterID == paramAzimuth)
    {
        // Update internal state
        pannerSettings.azimuth.store(newValue);

        // Only do coordinate conversion if azimuth is NOT currently owned by a UI control
        if (!azimuthOwnedByUI.load())
        {
            // Update dependent values using atomic loads
            float azimuth = pannerSettings.azimuth.load();
            float diverge = pannerSettings.diverge.load();
            float newX, newY;
            convertRCtoXYRaw(azimuth, diverge, newX, newY);
            pannerSettings.x.store(newX);
            pannerSettings.y.store(newY);
        }
    }
    else if (parameterID == paramElevation)
    {
        // Update internal state
        pannerSettings.elevation.store(newValue); // update pannerSettings value from host
    }
    else if (parameterID == paramDiverge)
    {
        // Update internal state
        pannerSettings.diverge.store(newValue); // update pannerSettings value from host

        // Only do coordinate conversion if diverge is NOT currently owned by a UI control
        if (!divergeOwnedByUI.load())
        {
            // Convert atomic values to local variables for function call
            float azimuth = pannerSettings.azimuth.load();
            float diverge = pannerSettings.diverge.load();
            float newX, newY;
            convertRCtoXYRaw(azimuth, diverge, newX, newY);
            pannerSettings.x.store(newX);
            pannerSettings.y.store(newY);
        }
    }
    else if (parameterID == paramGain)
    {
        pannerSettings.gain.store(newValue); // update pannerSettings value from host
    }
    else if (parameterID == paramAutoOrbit)
    {
        if (pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputMode::Stereo)
        { // if stereo mode
            pannerSettings.autoOrbit.store((bool)newValue); // update pannerSettings value from host
            // reset stereo params when auto orbit is disabled
            if (!pannerSettings.autoOrbit.load())
            {
                pannerSettings.stereoOrbitAzimuth = 0.0f;
                pannerSettings.stereoSpread = 0.0f;
                pannerSettings.stereoInputBalance = 0.0f;
                pendingStereoParameterReset.store(true);
            }
        }
    }
    else if (parameterID == paramStereoOrbitAzimuth)
    {
        // Always update stereo orbit azimuth parameter regardless of input mode
        pannerSettings.stereoOrbitAzimuth.store(newValue); // update pannerSettings value from host
    }
    else if (parameterID == paramStereoSpread)
    {
        // Always update stereo spread parameter regardless of input mode
        pannerSettings.stereoSpread.store(newValue); // update pannerSettings value from host
    }
    else if (parameterID == paramStereoInputBalance)
    {
        // Always update stereo input balance parameter regardless of input mode
        pannerSettings.stereoInputBalance.store(newValue); // update pannerSettings value from host
    }
    else if (parameterID == paramIsotropicEncodeMode)
    {
        pannerSettings.isotropicMode.store((bool)newValue); // update pannerSettings value from host
    }
    else if (parameterID == paramEqualPowerEncodeMode)
    {
        pannerSettings.equalpowerMode.store((bool)newValue); // update pannerSettings value from host
    }
    else if (parameterID == paramInputMode)
    {
        // stop pro tools from using plugin data to change input after creation
        if (!hostType.isProTools() || (hostType.isProTools() && (getTotalNumInputChannels() == 4 || getTotalNumInputChannels() == 6)))
        {
            requestedInputMode.store(static_cast<int>(newValue));
            pendingModeChange.store(true);
        }
    }
    else if (parameterID == paramOutputMode)
    {
        // stop pro tools from using plugin data to change output after creation
        if (!hostType.isProTools() || (hostType.isProTools() && getTotalNumOutputChannels() > 8))
        {
            requestedOutputMode.store(static_cast<int>(newValue));
            pendingModeChange.store(true);
        }
    }
    else if (parameterID == paramGainCompensationMode)
    {
        pannerSettings.gainCompensationMode.store((bool)newValue);
    }
#ifdef ITD_PARAMETERS
    else if (parameterID == paramITDActive)
    {
        pannerSettings.itdActive = (bool)newValue;
    }
    else if (parameterID == paramDelayTime)
    {
        pannerSettings.delayTime = newValue;
    }
    else if (parameterID == paramDelayDistance)
    {
        pannerSettings.delayDistance = newValue;
    }
#endif
    else if (parameterID == "output_layout_lock")
    {
        pannerSettings.lockOutputLayout.store((bool)newValue);
        lockOutputLayout = (bool)newValue;
    }
    pendingPannerSettingsSend.store(true);
}

#ifndef CUSTOM_CHANNEL_LAYOUT
bool M1PannerAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    if (layouts.getMainInputChannelSet().isDisabled() ||
        layouts.getMainOutputChannelSet().isDisabled())
    {
        DBG("Layout REJECTED - Disabled buses");
        return false;
    }

    // If the host is Reaper always allow all configurations
    // Reaper supports flexible I/O resizing without re-initializing the plugin
    if (hostType.isReaper())
    {
        return true;
    }

    // If the host is Pro Tools only allow the standard bus configurations
    if (hostType.isProTools() || hostType.getPluginLoadedAs() == AudioProcessor::wrapperType_AAX)
    {
        // Using a compiler flag for instances of Pro Tools scanning plugins externally from the main application
        // This is a feature seen in 2024+ versions of Pro Tools
        bool validInput = (layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono() ||
                           layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo() ||
                           layouts.getMainInputChannelSet() == juce::AudioChannelSet::createLCR() ||
                           layouts.getMainInputChannelSet() == juce::AudioChannelSet::createLCRS() ||
                           layouts.getMainInputChannelSet() == juce::AudioChannelSet::quadraphonic() ||
                           layouts.getMainInputChannelSet() == juce::AudioChannelSet::ambisonic(1) ||
                           layouts.getMainInputChannelSet() == juce::AudioChannelSet::create5point0() ||
                           layouts.getMainInputChannelSet() == juce::AudioChannelSet::create5point1());

        bool validOutput = (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::quadraphonic() ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::create7point1() ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::create7point1point6() ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(2) ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(3) ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(4) ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(5) ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(6) ||
                            layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(7));

        DBG("Layout " + juce::String(validInput && validOutput ? "ACCEPTED" : "REJECTED") +
            " - Input: " + layouts.getMainInputChannelSet().getDescription() +
            " Output: " + layouts.getMainOutputChannelSet().getDescription());

        return validInput && validOutput;
    }

    // For standalone, only allow stereo in/out
    if (JUCEApplicationBase::isStandaloneApp() || hostType.isPluginval())
    {
        auto inputLayout = layouts.getMainInputChannelSet();
        auto outputLayout = layouts.getMainOutputChannelSet();

        bool isValid = ((inputLayout == juce::AudioChannelSet::mono() ||
                        inputLayout == juce::AudioChannelSet::stereo()) &&
                       outputLayout == juce::AudioChannelSet::stereo());

        DBG("Standalone Layout " + juce::String(isValid ? "ACCEPTED" : "REJECTED") +
            " - Input: " + inputLayout.getDescription() +
            " Output: " + outputLayout.getDescription());

        return isValid;
    }
    // EXTERNAL STREAMING MODE: Accept stereo in/out for streaming to external mixer
#if M1_ENABLE_EXTERNAL_RENDERER
    else if ((layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono() || 
              layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()) && 
             (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()))
    {
        DBG("Layout ACCEPTED for streaming mode - Input: " + layouts.getMainInputChannelSet().getDescription() +
            " Output: " + layouts.getMainOutputChannelSet().getDescription());
        return true;
    }
#endif
    else
    {
        // Test for all available Mach1Encode configs
        // manually maintained for-loop of first enum element to last enum element
        Mach1Encode<float> configTester;
        for (int inputEnum = Mach1EncodeInputMode::Mono; inputEnum != Mach1EncodeInputMode::FiveDotOneSMTPE; inputEnum++)
        {
            configTester.setInputMode(static_cast<Mach1EncodeInputMode>(inputEnum));
            // test each input, if the input has the number of channels as the input testing layout has move on to output testing
            if (layouts.getMainInputChannelSet().size() == configTester.getInputChannelsCount())
            {
                // Note: Change the max for loop output to max bus size when new formats are introduced
                for (int outputEnum = 0; outputEnum != Mach1EncodeOutputMode::M1Spatial_14; outputEnum++)
                {
                    configTester.setOutputMode(static_cast<Mach1EncodeOutputMode>(outputEnum));
                    if (layouts.getMainOutputChannelSet().size() == configTester.getOutputChannelsCount())
                    {
                        DBG("Layout ACCEPTED - Input: " + layouts.getMainInputChannelSet().getDescription() +
                            " Output: " + layouts.getMainOutputChannelSet().getDescription());
                        return true;
                    }
                }
            }
        }
        DBG("Layout REJECTED - No matching configuration found");
        return false;
    }
}
#endif

void M1PannerAudioProcessor::fillChannelOrderArray(int numM1OutputChannels)
{
    // sets the maximum channels of the current host layout
    juce::AudioChannelSet chanset = getBus(false, 0)->getCurrentLayout();
    int numHostOutputChannels = getBus(false, 0)->getNumberOfChannels();

    // sets the maximum channels of the current selected m1 output layout
    std::vector<juce::AudioChannelSet::ChannelType> chan_types;
    chan_types.resize(numM1OutputChannels);
    output_channel_indices.resize(numM1OutputChannels);

    if (external_spatialmixer_active)
    {
        // In external mixer mode, we process all channels internally
        // but only output the first few to the host
        for (int i = 0; i < numM1OutputChannels; ++i)
        {
            if (i < numHostOutputChannels)
            {
                // These channels are output to host
                output_channel_indices[i] = i;
            }
            else
            {
                // These channels are internal processing only
                // Use the channel index directly for internal processing
                output_channel_indices[i] = i;
            }
        }
    }
    else if (!chanset.isDiscreteLayout())
    { // Check for DAW specific instructions
        if (hostType.isProTools() && chanset.size() == 8 && chanset.getDescription().contains(juce::String("7.1 Surround")))
        {
            // TODO: Remove this and figure out why we cannot use what is in "else" on PT 7.1
            chan_types[0] = juce::AudioChannelSet::ChannelType::left;
            chan_types[1] = juce::AudioChannelSet::ChannelType::centre;
            chan_types[2] = juce::AudioChannelSet::ChannelType::right;
            chan_types[3] = juce::AudioChannelSet::ChannelType::leftSurroundSide;
            chan_types[4] = juce::AudioChannelSet::ChannelType::rightSurroundSide;
            chan_types[5] = juce::AudioChannelSet::ChannelType::leftSurroundRear;
            chan_types[6] = juce::AudioChannelSet::ChannelType::rightSurroundRear;
            chan_types[7] = juce::AudioChannelSet::ChannelType::LFE;
        }
        else
        {
            // Get the index of each channel supplied via JUCE
            for (int i = 0; i < numM1OutputChannels; i++)
            {
                chan_types[i] = chanset.getTypeOfChannel(i);
            }
        }
        // Apply the index
        for (int i = 0; i < numM1OutputChannels; i++)
        {
            output_channel_indices[i] = chanset.getChannelIndexForType(chan_types[i]);
        }

        // Debug output for channel ordering
        if (hostType.isProTools() && chanset.size() == 8)
        {
            juce::String debugStr = "Channel mapping: ";
            for (int i = 0; i < numM1OutputChannels; i++)
            {
                debugStr += "M1[" + juce::String(i) + "]->Host[" + juce::String(output_channel_indices[i]) + "] ";
            }
            DBG(debugStr);
        }
    }
    else
    { // is a discrete channel layout
        for (int i = 0; i < numM1OutputChannels; ++i)
        {
            output_channel_indices[i] = i;
        }
    }
}

void M1PannerAudioProcessor::updateM1EncodePoints()
{
    applyStateToEncode(pannerSettings.m1Encode, getUiReticleSnapshotState());
    float old_gain_comp = gain_comp_in_db;
    gain_comp_in_db = pannerSettings.m1Encode.getGainCompensation(true); // store new gain compensation

    // Debug output for gain compensation changes
    if (std::abs(old_gain_comp - gain_comp_in_db) > 0.1f)
    {
        DBG("Gain compensation changed: " + juce::String(old_gain_comp, 2) + " -> " + juce::String(gain_comp_in_db, 2) +
            " dB (Az: " + juce::String(pannerSettings.azimuth, 1) + "°, Div: " + juce::String(pannerSettings.diverge, 1) + "%)");
    }
}

void M1PannerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    
    // Early exit if plugin is being destroyed (state == -1)
    if (pannerSettings.state.load() == -1)
    {
        buffer.clear();
        return;
    }

    // Use this method as the place to do any pre-playback
    if (!layoutCreated.load())
    {
        // Only attempt layout creation if buses are properly initialized
        if (getBus(false, 0) && getBus(true, 0))
        {
            createLayout(); // this should only be called here after initialization to avoid threading issues
        }
        else
        {
            // Buses not ready yet - clear buffer and return early to avoid VST3 buffer mapping issues
            DBG("[PANNER] Buses not ready in processBlock, clearing buffer");
            buffer.clear();
            return;
        }
    }

    // Initialize memory sharing if external spatial mixer is active
#if M1_ENABLE_EXTERNAL_RENDERER
    if (external_spatialmixer_active && !m_memoryShareInitialized)
        initializeMemorySharing();
#endif

    if (needToUpdateM1EncodePoints.load())
    {
        updateM1EncodePoints();
        needToUpdateM1EncodePoints.store(false);
    }

    // Update the host playhead data external usage
#if M1_ENABLE_EXTERNAL_RENDERER
    if (external_spatialmixer_active && getPlayHead() != nullptr)
    {
        juce::AudioPlayHead* ph = getPlayHead();
        juce::AudioPlayHead::CurrentPositionInfo currentPlayHeadInfo;
        if (ph->getCurrentPosition(currentPlayHeadInfo))
        {
            hostTimelineData.isPlaying = currentPlayHeadInfo.isPlaying;
            hostTimelineData.playheadPositionInSeconds = currentPlayHeadInfo.timeInSeconds;
        }
    }
#endif

    // Safety check: Ensure coefficients are initialized before processing
    if (smoothedChannelCoeffs.empty() || smoothedChannelCoeffs.size() == 0)
    {
        DBG("[PANNER] Warning: processBlock called before coefficients initialized, clearing buffer");
        buffer.clear();
        return;
    }

    // Set m1Encode obj values for processing
    auto gainCoeffs = pannerSettings.m1Encode.getGains();

    if (gainCoeffs.empty() || gainCoeffs.size() == 0)
    {
        DBG("[PANNER] Warning: gainCoeffs not initialized, clearing buffer");
        buffer.clear();
        return;
    }

    // vector of input channel buffers
    juce::AudioSampleBuffer mainInput = getBusBuffer(buffer, true, 0);
    juce::AudioChannelSet inputLayout = getChannelLayoutOfBus(true, 0);

    // Share input audio data BEFORE any processing (for external spatial mixer)
#if M1_ENABLE_EXTERNAL_RENDERER
    if (external_spatialmixer_active && m_memoryShareInitialized)
    {
        updateMemorySharing(mainInput);
    }
    else if (external_spatialmixer_active && !m_memoryShareInitialized)
    {
        DBG("[M1MemoryShare] Memory sharing active but not initialized yet");
    }
#endif

    // output buffers
    juce::AudioSampleBuffer mainOutput = getBusBuffer(buffer, false, 0);

#ifdef ITD_PARAMETERS
    mDelayTimeSmoother.setTargetValue(delayTimeParameter->get());
    const float udtime = mDelayTimeSmoother.getNextValue() * mSampleRate / 1000000; // number of samples in a microsecond * number of microseconds
#endif

    // input pan balance for stereo input
    if (mainInput.getNumChannels() > 1 && pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputMode::Stereo)
    {
        // Only apply stereo input balance if it's not at default (0)
        if (std::abs(pannerSettings.stereoInputBalance) > 0.001f)
        {
            float p = juce::MathConstants<float>::pi * (pannerSettings.stereoInputBalance + 1) / 4;
            mainInput.applyGain(0, 0, buffer.getNumSamples(), std::cos(p)); // gain for Left
            mainInput.applyGain(1, 0, buffer.getNumSamples(), std::sin(p)); // gain for Right
        }
    }

    // resize the processing buffer and zero it out so it works despite how many of the expected channels actually exist host side
    audioDataIn.resize(mainInput.getNumChannels()); // resizing the process data to what the host can support
    // TODO: error handle for when requested m1Encode input size is more than the host supports
    for (int input_channel = 0; input_channel < mainInput.getNumChannels(); input_channel++)
    {
        audioDataIn[input_channel].resize(buffer.getNumSamples(), 0.0);
    }

    // input channel setup loop
    for (int input_channel = 0; input_channel < pannerSettings.m1Encode.getInputChannelsCount(); input_channel++)
    {
        if (input_channel > mainInput.getNumChannels() - 1)
        {
            // Input channel is missing, set its gains to zero
            for (int output_channel = 0; output_channel < pannerSettings.m1Encode.getOutputChannelsCount(); output_channel++)
            {
                smoothedChannelCoeffs[input_channel][output_channel].setTargetValue(0.0f);
            }
        }
        else
        {
            // Copy input data to additional buffer
            memcpy(audioDataIn[input_channel].data(), mainInput.getReadPointer(input_channel), sizeof(float) * buffer.getNumSamples());

            // output channel setup loop
            for (int output_channel = 0; output_channel < pannerSettings.m1Encode.getOutputChannelsCount(); output_channel++)
            {
                // Set coefficients using M1 channel order (reordering applied later)
                smoothedChannelCoeffs[input_channel][output_channel].setTargetValue(gainCoeffs[input_channel][output_channel]);
            }
        }
    }

    // multichannel temp buffer (also used for informing meters even when not processing to write pointers
    // Note: Use buf.getNumChannels() for output size from this point on to not mismatch from new m1Encode size requests
    juce::AudioBuffer<float> buf(pannerSettings.m1Encode.getOutputChannelsCount(), buffer.getNumSamples());
    buf.clear();
    // multichannel output buffer (if internal processing is active this will have the above copy into it)
    float* const* outBuffer = mainOutput.getArrayOfWritePointers();

    // prepare the output buffer - clear all channels efficiently
    mainOutput.clear();

    // processing loop
    for (int input_channel = 0; input_channel < pannerSettings.m1Encode.getInputChannelsCount(); input_channel++)
    {
        if (input_channel > mainInput.getNumChannels() - 1)
        {
            // Skip processing for missing input channels
            DBG("SKIPPING Input[" + juce::String(input_channel) + "] - missing input channel");
            continue;
        }

        // Skip processing if channel is muted
        if (channelMuteStates[input_channel])
        {
            DBG("SKIPPING Input[" + juce::String(input_channel) + "] - channel muted");
            continue;
        }

        for (int sample = 0; sample < buffer.getNumSamples(); sample++)
        {
            // break if expected input channel num size does not match current input channel num size from host
            if (input_channel > mainInput.getNumChannels() - 1)
            {
                break;
            }
            else
            {
                // Get each input sample per channel
                float inValue = audioDataIn[input_channel][sample];

                // Apply to each of the output channels per input channel
                for (int output_channel = 0; output_channel < buf.getNumChannels(); output_channel++)
                {
                    // Output channel reordering from fillChannelOrder()
                    int output_channel_reordered = output_channel_indices[output_channel];

                    // process via temp buffer that will also be used for meters
                    if (output_channel_reordered >= 0)
                    {
                        // Get the next Mach1Encode coeff
                        float spatialGainCoeff = smoothedChannelCoeffs[input_channel][output_channel].getNextValue();
                        buf.addSample(output_channel, sample, inValue * spatialGainCoeff);
                    }

                    // Copy processed sample to host output buffer if channel exists in host (for internal processing mode)
                    if (!external_spatialmixer_active && output_channel_reordered < mainOutput.getNumChannels())
                    {
                        /// ANYTHING THAT IS ONLY FOR INTERNAL MULTICHANNEL PROCESSING GOES HERE
                        if (output_channel > mainOutput.getNumChannels() - 1)
                        {
                            // TODO: Test for external_mixer?
                            break;
                        }
                        else
                        {
#ifdef ITD_PARAMETERS
                        //SIMPLE DELAY
                        // scale delayCoeffs to be normalized
                        for (int i = 0; i < pannerSettings.m1Encode.getInputChannelsCount(); i++)
                        {
                            for (int o = 0; o < pannerSettings.m1Encode.getOutputChannelsCount(); o++)
                            {
                                delayCoeffs[i][o] = std::min(0.25f, delayCoeffs[i][o]); // clamp maximum to .25f
                                delayCoeffs[i][o] *= 4.0f; // rescale range to 0.0->1.0
                                // Incorporate the distance delay multiplier
                                // using min to correlate delayCoeffs as multiplier increases
                                //delayCoeffs[i][o] = std::min<float>(1.0f, (delayCoeffs[i][o]+0.01f) * (float)delayDistanceParameter->get()/100.);
                                //delayCoeffs[i][o] *= delayDistanceParameter->get()/10.;
                            }
                        }

                        if ((bool)*itdParameter)
                        {
                            for (int sample = 0; sample < numSamples; sample++)
                            {
                                // write original to delay
                                float udtime = mDelayTimeSmoother.getNextValue() * mSampleRate / 1000000; // number of samples in a microsecond * number of microseconds
                                for (auto channel = 0; channel < pannerSettings.m1Encode.getOutputChannelsCount(); channel++)
                                {
                                    ring->pushSample(channel, outBuffer[channel][sample]);
                                }
                                for (int channel = 0; channel < pannerSettings.m1Encode.getOutputChannelsCount(); channel++)
                                {
                                    outBuffer[channel][sample] = (outBuffer[channel][sample] * 0.707106781) + (ring->getSampleAtDelay(channel, udtime * delayCoeffs[0][channel]) * 0.707106781); // pan-law applied via `0.707106781`
                                }
                                ring->increment();
                            }
#endif // end of ITD_PARAMETERS
                        }
                    }
                }
            }
        }
    }

    // In external mixer mode, output SILENCE to avoid double-monitoring
    // (the spatial audio goes to m1-system-helper via M1MemoryShare, not to host output)
    if (external_spatialmixer_active)
    {
        // Output silence - audio is already sent via M1MemoryShare to external mixer
        mainOutput.clear();
        // Note: If a "Local Monitor (Dry)" parameter is added in the future,
        // we could optionally pass through the dry input signal here instead of silence
    }
    else
    {
        // Apply channel reordering to the output buffer
        for (int output_channel = 0; output_channel < buf.getNumChannels(); output_channel++)
        {
            int output_channel_reordered = output_channel_indices[output_channel];
            if (output_channel_reordered >= 0)
            {
                for (int sample = 0; sample < buffer.getNumSamples(); sample++)
                {
                    mainOutput.addSample(output_channel_reordered, sample, buf.getSample(output_channel, sample));
                }
            }
        }
    }

    // update meters - for external mixer mode, show all internal processing channels
    if (external_spatialmixer_active)
    {
        outputMeterValuedB.resize(buf.getNumChannels()); // expand meter UI number
        for (int output_channel = 0; output_channel < buf.getNumChannels(); output_channel++)
        {
            int output_channel_reordered = output_channel_indices[output_channel];
            // All channels should now have valid indices (0-7 for 8-channel processing)
            outputMeterValuedB.set(output_channel, juce::Decibels::gainToDecibels(buf.getRMSLevel(output_channel, 0, buf.getNumSamples())));
        }
    }
    else
    {
        // update meters for internal processing mode
        outputMeterValuedB.resize(mainOutput.getNumChannels()); // expand meter UI number
        for (int output_channel = 0; output_channel < mainOutput.getNumChannels(); output_channel++)
        {
            outputMeterValuedB.set(output_channel, output_channel < mainOutput.getNumChannels() ? juce::Decibels::gainToDecibels(mainOutput.getRMSLevel(output_channel, 0, buffer.getNumSamples())) : -144);
        }
    }
}

void M1PannerAudioProcessor::timerCallback()
{
    applyPendingModeChange();
    applyPendingStereoParameterReset();

    pannerOSC->update(); // test for connection

#if M1_ENABLE_EXTERNAL_RENDERER
    if (!external_spatialmixer_active && m_memoryShareInitialized && m_memoryShare && m_memoryShare->isValid())
        updateMemorySharingParametersOnly();
#endif

    if (pendingPannerSettingsSend.load() && pannerOSC->isConnected() && sendCurrentPannerSettings())
    {
        pendingPannerSettingsSend.store(false);
    }

    // Periodic health check: ensure helper is running (every ~10s at 200ms timer)
    static int healthCheckCounter = 0;
    if (++healthCheckCounter >= 50) {
        healthCheckCounter = 0;
        auto& helperManager = Mach1::M1SystemHelperManager::getInstance();
        if (!helperManager.isHelperServiceRunning()) {
            helperManager.requestHelperService("M1-Panner");
        }
    }
}

//==============================================================================
juce::AudioProcessorValueTreeState& M1PannerAudioProcessor::getValueTreeState()
{
    return parameters;
}

bool M1PannerAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* M1PannerAudioProcessor::createEditor()
{
    return new M1PannerAudioProcessorEditor(*this);
}

void M1PannerAudioProcessor::convertRCtoXYRaw(float r, float d, float& x, float& y)
{
    x = cos(juce::degreesToRadians(-r + 90)) * d * sqrt(2);
    y = sin(juce::degreesToRadians(-r + 90)) * d * sqrt(2);
    if (x > 100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { 100, -100, 100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (y > 100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { -100, 100, 100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (x < -100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { -100, -100, -100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (y < -100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { -100, -100, 100, -100 });
        x = intersection.x;
        y = intersection.y;
    }
}

void M1PannerAudioProcessor::convertXYtoRCRaw(float x, float y, float& r, float& d)
{
    if (x == 0 && y == 0)
    {
        r = 0;
        d = 0;
    }
    else
    {
        d = sqrtf(x * x + y * y) / sqrt(2.0);
        float rotation_radian = atan2(x, y); //acos(x/d);
        r = juce::radiansToDegrees(rotation_radian);
    }
}

void M1PannerAudioProcessor::m1EncodeChangeInputOutputMode(Mach1EncodeInputMode inputMode, Mach1EncodeOutputMode outputMode)
{
    if (pannerSettings.m1Encode.getOutputMode() != outputMode)
    {
        DBG("Current config: " + std::to_string(pannerSettings.m1Encode.getOutputMode()) + " and new config: " + std::to_string(outputMode));
        pannerSettings.m1Encode.setOutputMode(outputMode);
        gain_comp_in_db = pannerSettings.m1Encode.getGainCompensation(true); // store new gain compensation
    }
    pannerSettings.m1Encode.setInputMode(inputMode);

#if M1_ENABLE_EXTERNAL_RENDERER
    m_cachedInputMode.store(static_cast<int>(inputMode));
    m_cachedOutputMode.store(static_cast<int>(outputMode));
#endif

    auto inputChannelsCount = pannerSettings.m1Encode.getInputChannelsCount();
    auto outputChannelsCount = pannerSettings.m1Encode.getOutputChannelsCount();

    // Initialize all channels as unmuted
    channelMuteStates.resize(inputChannelsCount, false);

    // Size smoothedChannelCoeffs to M1 canonical channel count
    smoothedChannelCoeffs = std::vector<std::vector<juce::LinearSmoothedValue<float>>>(inputChannelsCount, std::vector<juce::LinearSmoothedValue<float>>(outputChannelsCount));
    for (int in = 0; in < inputChannelsCount; ++in)
    {
        for (int out = 0; out < outputChannelsCount; ++out)
        {
            smoothedChannelCoeffs[in][out].setCurrentAndTargetValue(smoothedChannelCoeffs[in][out].getTargetValue());
        }
    }
    output_channel_indices.resize(outputChannelsCount);

    // Checks if output bus is non DISCRETE layout and fixes host specific channel ordering issues
    fillChannelOrderArray(outputChannelsCount);

    for (int input_channel = 0; input_channel < inputChannelsCount; input_channel++)
    {
        smoothedChannelCoeffs[input_channel] = std::vector<juce::LinearSmoothedValue<float>>();
        smoothedChannelCoeffs[input_channel].resize(outputChannelsCount);
        for (int output_channel = 0; output_channel < outputChannelsCount; output_channel++)
        {
            smoothedChannelCoeffs[input_channel][output_channel].reset(processorSampleRate, (double)0.01);
        }
    }

    needToUpdateM1EncodePoints.store(true); // need to call to update the m1encode obj for new point counts
    uiReticleSnapshotDirty.store(true);
}

M1PannerAudioProcessor::UiReticleSnapshotState M1PannerAudioProcessor::getUiReticleSnapshotState()
{
    UiReticleSnapshotState state;
    state.azimuth = pannerSettings.azimuth;
    state.elevation = pannerSettings.elevation;
    state.diverge = pannerSettings.diverge;
    state.gain = pannerSettings.gain;
    state.stereoOrbitAzimuth = pannerSettings.stereoOrbitAzimuth;
    state.stereoSpread = pannerSettings.stereoSpread;
    state.autoOrbit = pannerSettings.autoOrbit;
    state.isotropicMode = pannerSettings.isotropicMode;
    state.equalpowerMode = pannerSettings.equalpowerMode;
    state.gainCompensationMode = pannerSettings.gainCompensationMode;
    state.monitorMode = monitorSettings.monitor_mode;
    state.inputMode = static_cast<int>(pannerSettings.m1Encode.getInputMode());
    state.outputMode = static_cast<int>(pannerSettings.m1Encode.getOutputMode());
    return state;
}

void M1PannerAudioProcessor::applyStateToEncode(Mach1Encode<float>& encode, const UiReticleSnapshotState& state)
{
    float diverge = state.diverge;
    float gain = state.gain;

    encode.setInputMode(static_cast<Mach1EncodeInputMode>(state.inputMode));
    encode.setOutputMode(static_cast<Mach1EncodeOutputMode>(state.outputMode));

    if (state.monitorMode == 1)
    {
        const float absDiverge = fabsf((diverge - -100.0f) / (100.0f - -100.0f));
        gain -= absDiverge * 6.0f;
        diverge = 0.0f;
    }

    encode.setAzimuthDegrees(state.azimuth);
    encode.setElevationDegrees(state.elevation);
    encode.setDiverge(diverge / 100.0f);
    encode.setOutputGain(gain, true);
    encode.setAutoOrbit(state.autoOrbit);
    encode.setOrbitRotationDegrees(state.stereoOrbitAzimuth);
    encode.setStereoSpread(state.stereoSpread / 100.0f);
    encode.setGainCompensationActive(state.gainCompensationMode);

    if (state.isotropicMode)
    {
        if (state.equalpowerMode)
        {
            encode.setPannerMode(Mach1EncodePannerMode::IsotropicEqualPower);
        }
        else
        {
            encode.setPannerMode(Mach1EncodePannerMode::IsotropicLinear);
        }
    }
    else
    {
        encode.setPannerMode(Mach1EncodePannerMode::PeriphonicLinear);
    }

    encode.generatePointResults();
}

void M1PannerAudioProcessor::refreshUiReticleSnapshotIfNeeded()
{
    const auto state = getUiReticleSnapshotState();
    if (!uiReticleSnapshotDirty.load() && areUiReticleSnapshotStatesEqual(state, lastUiReticleSnapshotState))
    {
        return;
    }

    Mach1Encode<float> snapshotEncode;
    applyStateToEncode(snapshotEncode, state);

    auto points = snapshotEncode.getPoints();
    auto names = snapshotEncode.getPointsNames();

    {
        const juce::ScopedLock snapshotLock(uiReticleSnapshotLock);
        uiReticlePoints = std::move(points);
        uiReticlePointNames = std::move(names);
    }

    lastUiReticleSnapshotState = state;
    uiReticleSnapshotDirty.store(false);
}

void M1PannerAudioProcessor::getUiReticleSnapshot(std::vector<Mach1Point3D>& points, std::vector<std::string>& names)
{
    refreshUiReticleSnapshotIfNeeded();

    const juce::ScopedLock snapshotLock(uiReticleSnapshotLock);
    points = uiReticlePoints;
    names = uiReticlePointNames;
}

bool M1PannerAudioProcessor::sendCurrentPannerSettings()
{
    try
    {
        const std::string track_name = track_properties.name.has_value() ? track_properties.name->toStdString() : "";
        const int panner_mode = pannerSettings.isotropicMode
            ? static_cast<int>(pannerSettings.equalpowerMode ? Mach1EncodePannerMode::IsotropicEqualPower
                                                             : Mach1EncodePannerMode::IsotropicLinear)
            : static_cast<int>(Mach1EncodePannerMode::PeriphonicLinear);
        return pannerOSC->sendPannerSettings(pannerSettings.state,
                                             track_name,
                                             osc_colour,
                                             static_cast<int>(pannerSettings.m1Encode.getInputMode()),
                                             pannerSettings.azimuth,
                                             pannerSettings.elevation,
                                             pannerSettings.diverge,
                                             pannerSettings.gain,
                                             panner_mode,
                                             pannerSettings.gainCompensationMode,
                                             pannerSettings.autoOrbit,
                                             pannerSettings.stereoOrbitAzimuth,
                                             pannerSettings.stereoSpread);
    }
    catch (...)
    {
        return false;
    }
}

//==============================================================================
juce::XmlElement* addXmlElement(juce::XmlElement& root, juce::String paramName, juce::String value)
{
    juce::XmlElement* el = root.createNewChildElement("param_" + paramName);
    el->setAttribute("value", juce::String(value));
    return el;
}

double getParameterDoubleFromXmlElement(juce::XmlElement* xml, juce::String paramName, double defVal)
{
    if (xml->getChildByName("param_" + paramName) && xml->getChildByName("param_" + paramName)->hasAttribute("value"))
    {
        double val = xml->getChildByName("param_" + paramName)->getDoubleAttribute("value", defVal);
        if (std::isnan(val))
        {
            return defVal;
        }
        return val;
    }
    return defVal;
}

int getParameterIntFromXmlElement(juce::XmlElement* xml, juce::String paramName, int defVal)
{
    if (xml->getChildByName("param_" + paramName) && xml->getChildByName("param_" + paramName)->hasAttribute("value"))
    {
        return xml->getChildByName("param_" + paramName)->getDoubleAttribute("value", defVal);
    }
    return defVal;
}

void M1PannerAudioProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    auto state = parameters.copyState();
    state.setProperty("trackColor_r", osc_colour.red, nullptr);
    state.setProperty("trackColor_g", osc_colour.green, nullptr);
    state.setProperty("trackColor_b", osc_colour.blue, nullptr);
    state.setProperty("trackColor_a", osc_colour.alpha, nullptr);
    state.setProperty("output_layout_lock", pannerSettings.lockOutputLayout.load(), nullptr);
#if M1_ENABLE_EXTERNAL_RENDERER
    if (m_instanceBaseName.isNotEmpty())
        state.setProperty("memory_instance_name", m_instanceBaseName, nullptr);
#endif
    if (auto xml = state.createXml())
    {
        copyXmlToBinary(*xml, destData);
    }
}

void M1PannerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary(data, sizeInBytes))
    {
        auto restoredState = juce::ValueTree::fromXml(*xml);

        if (restoredState.isValid() && restoredState.hasType(parameters.state.getType()))
        {
            parameters.replaceState(restoredState);

            osc_colour.red = (int) restoredState.getProperty("trackColor_r", osc_colour.red);
            osc_colour.green = (int) restoredState.getProperty("trackColor_g", osc_colour.green);
            osc_colour.blue = (int) restoredState.getProperty("trackColor_b", osc_colour.blue);
            osc_colour.alpha = (int) restoredState.getProperty("trackColor_a", osc_colour.alpha);
            const bool restoredLockOutputLayout = static_cast<bool>(
                restoredState.getProperty("output_layout_lock",
                                          juce::var(pannerSettings.lockOutputLayout.load())));
            pannerSettings.lockOutputLayout.store(restoredLockOutputLayout);
            lockOutputLayout = restoredLockOutputLayout;
#if M1_ENABLE_EXTERNAL_RENDERER
            m_instanceBaseName = restoredState.getProperty("memory_instance_name", m_instanceBaseName).toString();
#endif

            needToUpdateM1EncodePoints.store(true);
            uiReticleSnapshotDirty.store(true);
            pendingPannerSettingsSend.store(true);
            return;
        }
    }

    // Restore the parameters from the memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::MemoryInputStream input(data, sizeInBytes, false);
    auto prefix = input.readString();

    /*
     This prefix string check is to define when we swap from mState parameters to newer AVPTS, using this to check if the plugin
     was made before this release version (1.5.1) since it would still be using mState, if it is a M1-Panner made before "1.5.1"
     then we no longer support recall of those plugin parameters.
     */
    if (!prefix.isEmpty())
    {
        juce::XmlDocument doc(input.readString());
        std::unique_ptr<juce::XmlElement> root(doc.getDocumentElement());
        auto& params = getValueTreeState();

        const auto restoredAzimuth = (float) getParameterDoubleFromXmlElement(root.get(), paramAzimuth, pannerSettings.azimuth);
        const auto restoredElevation = (float) getParameterDoubleFromXmlElement(root.get(), paramElevation, pannerSettings.elevation);
        const auto restoredDiverge = (float) getParameterDoubleFromXmlElement(root.get(), paramDiverge, pannerSettings.diverge);
        const auto restoredGain = (float) getParameterDoubleFromXmlElement(root.get(), paramGain, pannerSettings.gain);
        const auto restoredStereoOrbitAzimuth = (float) getParameterDoubleFromXmlElement(root.get(), paramStereoOrbitAzimuth, pannerSettings.stereoOrbitAzimuth);
        const auto restoredStereoSpread = (float) getParameterDoubleFromXmlElement(root.get(), paramStereoSpread, pannerSettings.stereoSpread);
        const auto restoredStereoInputBalance = (float) getParameterDoubleFromXmlElement(root.get(), paramStereoInputBalance, pannerSettings.stereoInputBalance);
        const auto restoredAutoOrbit = (bool) getParameterIntFromXmlElement(root.get(), paramAutoOrbit, pannerSettings.autoOrbit);
        const auto restoredIsotropicMode = (bool) getParameterIntFromXmlElement(root.get(), paramIsotropicEncodeMode, pannerSettings.isotropicMode);
        const auto restoredEqualPowerMode = (bool) getParameterIntFromXmlElement(root.get(), paramEqualPowerEncodeMode, pannerSettings.equalpowerMode);
        const auto restoredGainCompensationMode = (float) getParameterDoubleFromXmlElement(root.get(), paramGainCompensationMode, pannerSettings.gainCompensationMode);
        const auto restoredInputMode = getParameterIntFromXmlElement(root.get(), paramInputMode, pannerSettings.m1Encode.getInputMode());
        const auto restoredOutputMode = getParameterIntFromXmlElement(root.get(), paramOutputMode, pannerSettings.m1Encode.getOutputMode());

        // update local parameters first
        parameterChanged(paramAzimuth, restoredAzimuth);
        parameterChanged(paramElevation, restoredElevation);
        parameterChanged(paramDiverge, restoredDiverge);
        parameterChanged(paramGain, restoredGain);
        parameterChanged(paramStereoOrbitAzimuth, restoredStereoOrbitAzimuth);
        parameterChanged(paramStereoSpread, restoredStereoSpread);
        parameterChanged(paramStereoInputBalance, restoredStereoInputBalance);
        parameterChanged(paramAutoOrbit, restoredAutoOrbit);
        parameterChanged(paramIsotropicEncodeMode, restoredIsotropicMode);
        parameterChanged(paramEqualPowerEncodeMode, restoredEqualPowerMode);
        parameterChanged(paramGainCompensationMode, restoredGainCompensationMode);

#ifdef ITD_PARAMETERS
        const auto restoredITDActive = (bool) getParameterIntFromXmlElement(root.get(), paramITDActive, pannerSettings.itdActive);
        const auto restoredDelayTime = (float) getParameterDoubleFromXmlElement(root.get(), paramDelayTime, pannerSettings.delayTime);
        const auto restoredDelayDistance = (float) getParameterDoubleFromXmlElement(root.get(), paramDelayDistance, pannerSettings.delayDistance);
        parameterChanged(paramITDActive, restoredITDActive);
        parameterChanged(paramDelayTime, restoredDelayTime);
        parameterChanged(paramDelayDistance, restoredDelayDistance);
#endif

        // Extras
        osc_colour.red = (int)getParameterIntFromXmlElement(root.get(), "trackColor_r", osc_colour.red);
        osc_colour.green = (int)getParameterIntFromXmlElement(root.get(), "trackColor_g", osc_colour.green);
        osc_colour.blue = (int)getParameterIntFromXmlElement(root.get(), "trackColor_b", osc_colour.blue);
        osc_colour.alpha = (int)getParameterIntFromXmlElement(root.get(), "trackColor_a", osc_colour.alpha);
        parameterChanged("output_layout_lock",
                         (bool) getParameterIntFromXmlElement(root.get(),
                                                              "output_layout_lock",
                                                              (int) pannerSettings.lockOutputLayout.load()));

        // Restore memory sharing instance identifier
#if M1_ENABLE_EXTERNAL_RENDERER
        if (root.get()->getChildByName("param_memory_instance_name") &&
            root.get()->getChildByName("param_memory_instance_name")->hasAttribute("value"))
        {
            m_instanceBaseName = root.get()->getChildByName("param_memory_instance_name")->getStringAttribute("value");
        }
#endif

        // if the parsed input from xml is not the default value
        parameterChanged(paramInputMode, Mach1EncodeInputMode(restoredInputMode));
        parameterChanged(paramOutputMode, Mach1EncodeOutputMode(restoredOutputMode));
        params.getParameter(paramInputMode)->setValueNotifyingHost(params.getParameter(paramInputMode)->convertTo0to1(restoredInputMode));
        params.getParameter(paramOutputMode)->setValueNotifyingHost(params.getParameter(paramOutputMode)->convertTo0to1(restoredOutputMode));

        // Update all parameters through the value tree
        params.getParameter(paramAzimuth)->setValueNotifyingHost(params.getParameter(paramAzimuth)->convertTo0to1(restoredAzimuth));
        params.getParameter(paramElevation)->setValueNotifyingHost(params.getParameter(paramElevation)->convertTo0to1(restoredElevation));
        params.getParameter(paramDiverge)->setValueNotifyingHost(params.getParameter(paramDiverge)->convertTo0to1(restoredDiverge));
        params.getParameter(paramGain)->setValueNotifyingHost(params.getParameter(paramGain)->convertTo0to1(restoredGain));
        params.getParameter(paramStereoOrbitAzimuth)->setValueNotifyingHost(params.getParameter(paramStereoOrbitAzimuth)->convertTo0to1(restoredStereoOrbitAzimuth));
        params.getParameter(paramStereoSpread)->setValueNotifyingHost(params.getParameter(paramStereoSpread)->convertTo0to1(restoredStereoSpread));
        params.getParameter(paramStereoInputBalance)->setValueNotifyingHost(params.getParameter(paramStereoInputBalance)->convertTo0to1(restoredStereoInputBalance));
        params.getParameter(paramAutoOrbit)->setValueNotifyingHost(params.getParameter(paramAutoOrbit)->convertTo0to1(restoredAutoOrbit));
        params.getParameter(paramIsotropicEncodeMode)->setValueNotifyingHost(params.getParameter(paramIsotropicEncodeMode)->convertTo0to1(restoredIsotropicMode));
        params.getParameter(paramEqualPowerEncodeMode)->setValueNotifyingHost(params.getParameter(paramEqualPowerEncodeMode)->convertTo0to1(restoredEqualPowerMode));
        params.getParameter(paramGainCompensationMode)->setValueNotifyingHost(params.getParameter(paramGainCompensationMode)->convertTo0to1(restoredGainCompensationMode));

#ifdef ITD_PARAMETERS
        params.getParameter(paramITDActive)->setValueNotifyingHost(params.getParameter(paramITDActive)->convertTo0to1(restoredITDActive));
        params.getParameter(paramDelayTime)->setValueNotifyingHost(params.getParameter(paramDelayTime)->convertTo0to1(restoredDelayTime));
        params.getParameter(paramDelayDistance)->setValueNotifyingHost(params.getParameter(paramDelayDistance)->convertTo0to1(restoredDelayDistance));
#endif
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new M1PannerAudioProcessor();
}

void M1PannerAudioProcessor::postAlert(const Mach1::AlertData& alert)
{
    if (postAlertToUI) {
        postAlertToUI(alert);
    } else {
        pendingAlerts.push_back(alert); // Store for later
        DBG("Stored alert for UI. Total pending: " + juce::String(pendingAlerts.size()));
    }
}

//==============================================================================
#if M1_ENABLE_EXTERNAL_RENDERER

juce::String M1PannerAudioProcessor::generateUniqueInstanceName() const
{
#ifdef JUCE_WINDOWS
    return juce::String("M1SpatialSystem_M1Panner_PID") + juce::String(GetCurrentProcessId()) + "_PTR" + juce::String::toHexString(reinterpret_cast<uintptr_t>(this)) + "_T" + juce::String(juce::Time::getCurrentTime().toMilliseconds());
#else
    return juce::String("M1SpatialSystem_M1Panner_PID") + juce::String(getpid()) + "_PTR" + juce::String::toHexString(reinterpret_cast<uintptr_t>(this)) + "_T" + juce::String(juce::Time::getCurrentTime().toMilliseconds());
#endif
}

void M1PannerAudioProcessor::initializeMemorySharing()
{
    try
    {
        // Generate unique base name for this instance (only if not restored from state)
        if (m_instanceBaseName.isEmpty())
        {
            m_instanceBaseName = generateUniqueInstanceName();
            DBG("[M1MemoryShare] Generated new memory instance name: " + m_instanceBaseName);
        }
        else
        {
            DBG("[M1MemoryShare] Using restored memory instance name: " + m_instanceBaseName);
        }

        // Calculate memory size needed (roughly 1MB for audio data with enhanced headers)
        size_t memorySize = 1024 * 1024; // 1MB

        // Create shared memory instance for audio data (with enhanced headers containing panner settings)
        m_memoryShare = std::make_unique<M1MemoryShare>(m_instanceBaseName, memorySize, true, true);

        if (m_memoryShare->isValid())
        {
            // Initialize for audio with current settings
            m_memoryShare->initializeForAudio(
                static_cast<uint32_t>(processorSampleRate),
                static_cast<uint32_t>(getMainBusNumInputChannels()),
                512 // Default block size, will be updated in prepareToPlay
            );

            m_memoryShareInitialized = true;

            DBG("[M1MemoryShare] Initialized successfully: " + m_instanceBaseName);
            DBG("[M1MemoryShare] Audio buffer headers now include panner settings and DAW timestamp");
        }
        else
        {
            DBG("[M1MemoryShare] Failed to initialize");
            m_memoryShare.reset();
        }
    }
    catch (const std::exception& e)
    {
        DBG("[M1MemoryShare] Exception during initialization: " + juce::String(e.what()));
        m_memoryShare.reset();
    }
}

void M1PannerAudioProcessor::updateMemorySharing(const juce::AudioBuffer<float>& inputBuffer)
{
    if (!m_memoryShare || !m_memoryShare->isValid())
        return;

    const int numInputChannels = inputBuffer.getNumChannels();
    if (numInputChannels != 1 && numInputChannels != 2)
        return;

    uint64_t dawTimestamp = static_cast<uint64_t>(juce::Time::currentTimeMillis());
    double playheadPosition = 0.0;
    bool isPlaying = false;

    // Get playhead info from DAW (this is safe in processBlock)
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo currentPlayHeadInfo;
        if (ph->getCurrentPosition(currentPlayHeadInfo))
        {
            isPlaying = currentPlayHeadInfo.isPlaying;
            playheadPosition = currentPlayHeadInfo.timeInSeconds;
        }
    }

    // Reuse pre-allocated map -- update values in-place (no heap allocations for existing keys)
    m_rtParameterMap.floatParams[M1PannerParameterIDs::AZIMUTH] = pannerSettings.azimuth.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::ELEVATION] = pannerSettings.elevation.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::DIVERGE] = pannerSettings.diverge.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::GAIN] = pannerSettings.gain.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::STEREO_ORBIT_AZIMUTH] = pannerSettings.stereoOrbitAzimuth.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::STEREO_SPREAD] = pannerSettings.stereoSpread.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::STEREO_INPUT_BALANCE] = pannerSettings.stereoInputBalance.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::AUTO_ORBIT] = pannerSettings.autoOrbit.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::ISOTROPIC_MODE] = pannerSettings.isotropicMode.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::EQUALPOWER_MODE] = pannerSettings.equalpowerMode.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::GAIN_COMPENSATION_MODE] = pannerSettings.gainCompensationMode.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::LOCK_OUTPUT_LAYOUT] = pannerSettings.lockOutputLayout.load();

    // Use cached input/output mode (updated by parameterChanged on message thread) -- no mutex needed
    m_rtParameterMap.intParams[M1PannerParameterIDs::INPUT_MODE] = m_cachedInputMode.load();
    m_rtParameterMap.intParams[M1PannerParameterIDs::OUTPUT_MODE] = m_cachedOutputMode.load();

    m_rtParameterMap.intParams[M1PannerParameterIDs::STATE] = pannerSettings.state.load();
    m_rtParameterMap.intParams[M1PannerParameterIDs::PORT] = pannerSettings.port;
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_R] = static_cast<int32_t>(pannerSettings.color.r.load());
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_G] = static_cast<int32_t>(pannerSettings.color.g.load());
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_B] = static_cast<int32_t>(pannerSettings.color.b.load());
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_A] = static_cast<int32_t>(pannerSettings.color.a.load());

    // DISPLAY_NAME: use cached string to avoid allocation (updated on timer thread)
    // The key already exists after first call, so this is an in-place update
    if (track_properties.name.has_value() && !track_properties.name->isEmpty())
        m_rtParameterMap.stringParams[M1PannerParameterIDs::DISPLAY_NAME] = track_properties.name->toStdString();
    else
        m_rtParameterMap.stringParams[M1PannerParameterIDs::DISPLAY_NAME] = "M1-Panner (" + std::to_string(pannerSettings.port) + ")";

    uint32_t currentSampleRate = static_cast<uint32_t>(processorSampleRate);
    if (currentSampleRate == 0) currentSampleRate = 44100;

    m_memoryShare->writeAudioBufferWithGenericParameters(inputBuffer, m_rtParameterMap, dawTimestamp,
                                                         playheadPosition, isPlaying, false, 1, currentSampleRate);
}

void M1PannerAudioProcessor::updateMemorySharingParametersOnly()
{
    if (!m_memoryShare || !m_memoryShare->isValid())
        return;

    uint64_t dawTimestamp = static_cast<uint64_t>(juce::Time::currentTimeMillis());
    double playheadPosition = 0.0;
    bool isPlaying = false;

    // Get playhead info from DAW if available
    if (auto* ph = getPlayHead())
    {
        juce::AudioPlayHead::CurrentPositionInfo currentPlayHeadInfo;
        if (ph->getCurrentPosition(currentPlayHeadInfo))
        {
            isPlaying = currentPlayHeadInfo.isPlaying;
            playheadPosition = currentPlayHeadInfo.timeInSeconds;
        }
    }

    // Reuse pre-allocated parameter map (timer thread -- allocations OK but keeping consistent)
    m_rtParameterMap.floatParams[M1PannerParameterIDs::AZIMUTH] = pannerSettings.azimuth.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::ELEVATION] = pannerSettings.elevation.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::DIVERGE] = pannerSettings.diverge.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::GAIN] = pannerSettings.gain.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::STEREO_ORBIT_AZIMUTH] = pannerSettings.stereoOrbitAzimuth.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::STEREO_SPREAD] = pannerSettings.stereoSpread.load();
    m_rtParameterMap.floatParams[M1PannerParameterIDs::STEREO_INPUT_BALANCE] = pannerSettings.stereoInputBalance.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::AUTO_ORBIT] = pannerSettings.autoOrbit.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::ISOTROPIC_MODE] = pannerSettings.isotropicMode.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::EQUALPOWER_MODE] = pannerSettings.equalpowerMode.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::GAIN_COMPENSATION_MODE] = pannerSettings.gainCompensationMode.load();
    m_rtParameterMap.boolParams[M1PannerParameterIDs::LOCK_OUTPUT_LAYOUT] = pannerSettings.lockOutputLayout.load();

    m_rtParameterMap.intParams[M1PannerParameterIDs::INPUT_MODE] = m_cachedInputMode.load();
    m_rtParameterMap.intParams[M1PannerParameterIDs::OUTPUT_MODE] = m_cachedOutputMode.load();

    m_rtParameterMap.intParams[M1PannerParameterIDs::STATE] = pannerSettings.state.load();
    m_rtParameterMap.intParams[M1PannerParameterIDs::PORT] = pannerSettings.port;
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_R] = static_cast<int32_t>(pannerSettings.color.r.load());
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_G] = static_cast<int32_t>(pannerSettings.color.g.load());
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_B] = static_cast<int32_t>(pannerSettings.color.b.load());
    m_rtParameterMap.intParams[M1PannerParameterIDs::COLOR_A] = static_cast<int32_t>(pannerSettings.color.a.load());

    if (track_properties.name.has_value() && !track_properties.name->isEmpty())
        m_rtParameterMap.stringParams[M1PannerParameterIDs::DISPLAY_NAME] = track_properties.name->toStdString();
    else
        m_rtParameterMap.stringParams[M1PannerParameterIDs::DISPLAY_NAME] = "M1-Panner (" + std::to_string(pannerSettings.port) + ")";

    juce::AudioBuffer<float> emptyBuffer(2, 0);
    m_memoryShare->writeAudioBufferWithGenericParameters(emptyBuffer, m_rtParameterMap, dawTimestamp, playheadPosition, isPlaying, false);

    // Touch the file modification time on the timer thread (not the audio thread)
    if (m_memoryShare)
        m_memoryShare->scheduleAsyncFileModTimeUpdate();
}

bool M1PannerAudioProcessor::isHelperServiceAvailable() const
{
    auto& helperManager = Mach1::M1SystemHelperManager::getInstance();
    return helperManager.isHelperServiceRunning();
}

#endif // M1_ENABLE_EXTERNAL_RENDERER
