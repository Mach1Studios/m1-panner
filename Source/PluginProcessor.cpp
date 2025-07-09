/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

/*
 Architecture:
    - parameterChanged() updates the i/o layout
    - parameterChanged() checks if matched with pannerSettings and otherwise updates this too
    - parameters expect normalized 0->1 except the i/o and pannerSettings which expects unnormalled values
 */

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
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramAzimuth, 1), TRANS("Azimuth"), juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), pannerSettings.azimuth, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "°"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramElevation, 1), TRANS("Elevation"), juce::NormalisableRange<float>(-90.0f, 90.0f, 0.01f), pannerSettings.elevation, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "°"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDiverge, 1), TRANS("Diverge"), juce::NormalisableRange<float>(-100.0f, 100.0f, 0.01f), pannerSettings.diverge, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1); }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramGain, 1), TRANS("Input Gain"), juce::NormalisableRange<float>(-90.0, 24.0f, 0.1f, std::log(0.5f) / std::log(100.0f / 106.0f)), pannerSettings.gain, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + " dB"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramAutoOrbit, 1), TRANS("Auto Orbit"), pannerSettings.autoOrbit),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoOrbitAzimuth, 1), TRANS("Stereo Orbit Azimuth"), juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), pannerSettings.stereoOrbitAzimuth, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "°"; }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoSpread, 1), TRANS("Stereo Spread"), juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), pannerSettings.stereoSpread, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1); }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoInputBalance, 1), TRANS("Stereo Input Balance"), juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.stereoInputBalance, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1); }, [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramIsotropicEncodeMode, 1), TRANS("Isotropic Encode Mode"), pannerSettings.isotropicMode),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramEqualPowerEncodeMode, 1), TRANS("Equal Power Encode Mode"), pannerSettings.equalpowerMode),
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramGainCompensationMode, 1), TRANS("Gain Compensation Mode"), pannerSettings.gainCompensationMode),
#ifndef CUSTOM_CHANNEL_LAYOUT
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID(paramInputMode, 1), TRANS("Input Mode"), 0, Mach1EncodeInputMode::BFOAACN, Mach1EncodeInputMode::Mono),
          // Note: Change init output to max bus size when new formats are introduced
          std::make_unique<juce::AudioParameterInt>(juce::ParameterID(paramOutputMode, 1), TRANS("Output Mode"), 0, Mach1EncodeOutputMode::M1Spatial_14, Mach1EncodeOutputMode::M1Spatial_8),
#endif
#ifdef ITD_PARAMETERS
          std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramITDActive, 1), TRANS("ITD"), pannerSettings.itdActive),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDelayTime, 1), TRANS("Delay Time (max)"), juce::NormalisableRange<float>(0.0f, 10000.0f, 1.0f), pannerSettings.delayTime, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + "μS"; }, [](const juce::String& t) { return t.dropLastCharacters(1).getFloatValue(); }),
          std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDelayDistance, 1), TRANS("Delay Distance"), juce::NormalisableRange<float>(0.0f, 10000.0f, 0.01f), pannerSettings.delayDistance, "", juce::AudioProcessorParameter::genericParameter, [](float v, int) { return juce::String(v, 1) + ""; }, [](const juce::String& t) { return t.dropLastCharacters(1).getFloatValue(); }),
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
    if (track_properties.colour.getAlpha() != 0)
    { // unfound colors are 0,0,0,0
        osc_colour.fromInt32(track_properties.colour.getARGB());
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
    startTimer(200);

    // print build time for debug
    juce::String date(__DATE__);
    juce::String time(__TIME__);
    DBG("[PANNER] Build date: " + date + " | Build time: " + time);
}

M1PannerAudioProcessor::~M1PannerAudioProcessor()
{
    pannerSettings.state = -1;
    stopTimer();
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
        DBG("Invalid bus configuration in createLayout()");
        return;
    }

    if (external_spatialmixer_active)
    {
        /// EXTERNAL MULTICHANNEL PROCESSING

        // INPUT
        if (pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputMode::Mono)
        {
            getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::mono());
        }
        else if (pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputMode::Stereo)
        {
            getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
        }
        // OUTPUT
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
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

    layoutCreated = true; // flow control for static i/o
    updateHostDisplay();
}

//==============================================================================
void M1PannerAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    if (!layoutCreated)
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
}

void M1PannerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void M1PannerAudioProcessor::parameterChanged(const juce::String& parameterID, float newValue)
{
    // Expects non-normalised values

    needToUpdateM1EncodePoints = true; // need to call to update the m1encode obj for new point counts

    if (parameterID == paramAzimuth)
    {
        // Update internal state
        pannerSettings.azimuth = newValue;
        // Update dependent values
        convertRCtoXYRaw(pannerSettings.azimuth, pannerSettings.diverge,
                        pannerSettings.x, pannerSettings.y);
    }
    else if (parameterID == paramElevation)
    {
        pannerSettings.elevation = newValue; // update pannerSettings value from host
        parameters.getParameter(paramElevation)->setValue(newValue);
    }
    else if (parameterID == paramDiverge)
    {
        pannerSettings.diverge = newValue; // update pannerSettings value from host
        parameters.getParameter(paramDiverge)->setValue(newValue);
        convertRCtoXYRaw(pannerSettings.azimuth, pannerSettings.diverge, pannerSettings.x, pannerSettings.y);
    }
    else if (parameterID == paramGain)
    {
        pannerSettings.gain = newValue; // update pannerSettings value from host
        parameters.getParameter(paramGain)->setValue(newValue);
    }
    else if (parameterID == paramAutoOrbit)
    {
        if (pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputMode::Stereo)
        { // if stereo mode
            pannerSettings.autoOrbit = (bool)newValue; // update pannerSettings value from host
            parameters.getParameter(paramAutoOrbit)->setValue((bool)newValue);
            // reset stereo params when auto orbit is disabled
            if (!pannerSettings.autoOrbit)
            {
                parameters.getParameter(paramStereoOrbitAzimuth)->setValue(0.0f);
                parameters.getParameter(paramStereoSpread)->setValue(0.0f);
                parameters.getParameter(paramStereoInputBalance)->setValue(0.0f);
            }
        }
    }
    else if (parameterID == paramStereoOrbitAzimuth)
    {
        // Always update stereo orbit azimuth parameter regardless of input mode
        pannerSettings.stereoOrbitAzimuth = newValue; // update pannerSettings value from host
        parameters.getParameter(paramStereoOrbitAzimuth)->setValue(newValue);
    }
    else if (parameterID == paramStereoSpread)
    {
        // Always update stereo spread parameter regardless of input mode
        pannerSettings.stereoSpread = newValue; // update pannerSettings value from host
        parameters.getParameter(paramStereoSpread)->setValue(newValue);
    }
    else if (parameterID == paramStereoInputBalance)
    {
        // Always update stereo input balance parameter regardless of input mode
        pannerSettings.stereoInputBalance = newValue; // update pannerSettings value from host
        parameters.getParameter(paramStereoInputBalance)->setValue(newValue);
    }
    else if (parameterID == paramIsotropicEncodeMode)
    {
        pannerSettings.isotropicMode = (bool)newValue; // update pannerSettings value from host
        parameters.getParameter(paramIsotropicEncodeMode)->setValue((bool)newValue);
    }
    else if (parameterID == paramEqualPowerEncodeMode)
    {
        pannerSettings.equalpowerMode = (bool)newValue; // update pannerSettings value from host
        parameters.getParameter(paramEqualPowerEncodeMode)->setValue((bool)newValue);
    }
    else if (parameterID == paramInputMode)
    {
        // stop pro tools from using plugin data to change input after creation
        if (!hostType.isProTools() || (hostType.isProTools() && (getTotalNumInputChannels() == 4 || getTotalNumInputChannels() == 6)))
        {
            Mach1EncodeInputMode inputType = Mach1EncodeInputMode((int)newValue);
            pannerSettings.m1Encode.setInputMode(inputType);
            parameters.getParameter(paramInputMode)->setValue(parameters.getParameter(paramInputMode)->convertTo0to1(newValue));
            layoutCreated = false;
        }
    }
    else if (parameterID == paramOutputMode)
    {
        // stop pro tools from using plugin data to change output after creation
        if (!hostType.isProTools() || (hostType.isProTools() && getTotalNumOutputChannels() > 8))
        {
            Mach1EncodeOutputMode outputType = Mach1EncodeOutputMode((int)newValue);
            pannerSettings.m1Encode.setOutputMode(outputType);
            gain_comp_in_db = pannerSettings.m1Encode.getGainCompensation(true); // store new gain compensation
            parameters.getParameter(paramOutputMode)->setValue(parameters.getParameter(paramOutputMode)->convertTo0to1(newValue));
            layoutCreated = false;
        }
    }
    else if (parameterID == paramGainCompensationMode)
    {
        pannerSettings.gainCompensationMode = newValue;
        parameters.getParameter(paramGainCompensationMode)->setValue(newValue);
    }
    else if (parameterID == "output_layout_lock")
    {
        pannerSettings.lockOutputLayout = (bool)newValue;
        lockOutputLayout = (bool)newValue;
    }
    // send a pannersettings update to helper since a parameter changed
    try {
        if (pannerOSC->isConnected())
        {
            pannerOSC->sendPannerSettings(pannerSettings.state, track_properties.name.toStdString(), osc_colour, (int)pannerSettings.m1Encode.getInputMode(), pannerSettings.azimuth, pannerSettings.elevation, pannerSettings.diverge, pannerSettings.gain, (int)pannerSettings.m1Encode.getPannerMode(), pannerSettings.gainCompensationMode, pannerSettings.autoOrbit, pannerSettings.stereoOrbitAzimuth, pannerSettings.stereoSpread);
        }
    }
    catch (...) {
        // TODO: Add error handling
    }
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
    /* TODO: Finish EXTERNAL STREAMING Mode before using this
    else if ((layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono() || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()) && (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()))
    {
        // RETURN TRUE FOR EXTERNAL STREAMING MODE
        // hard set {1,2} and {2,2} for streaming use case
        return true;
    }
    */
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

    if (!chanset.isDiscreteLayout())
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
    float _diverge = pannerSettings.diverge;
    float _gain = pannerSettings.gain;

    if (monitorSettings.monitor_mode == 1)
    { // StereoSafe mode is on
        //store diverge for gain
        float abs_diverge = fabsf((_diverge - -100.0f) / (100.0f - -100.0f));
        //Setup for stereoSafe diverge range to gain
        _gain = _gain - (abs_diverge * 6.0);
        //Set Diverge to 0 after using Diverge for Gain
        _diverge = 0;
    }

    // parameters that can be automated will get their values updated from PannerSettings->Parameter
    pannerSettings.m1Encode.setAzimuthDegrees(pannerSettings.azimuth);
    pannerSettings.m1Encode.setElevationDegrees(pannerSettings.elevation);
    pannerSettings.m1Encode.setDiverge(_diverge / 100); // using _diverge in case monitorMode was used
    pannerSettings.m1Encode.setOutputGain(pannerSettings.gain, true);
    float old_gain_comp = gain_comp_in_db;
    gain_comp_in_db = pannerSettings.m1Encode.getGainCompensation(true); // store new gain compensation

    pannerSettings.m1Encode.setAutoOrbit(pannerSettings.autoOrbit);
    pannerSettings.m1Encode.setOrbitRotationDegrees(pannerSettings.stereoOrbitAzimuth);
    pannerSettings.m1Encode.setStereoSpread(pannerSettings.stereoSpread / 100.0); // Mach1Encode expects an unsigned normalized input
    pannerSettings.m1Encode.setGainCompensationActive(pannerSettings.gainCompensationMode);

    // Debug output for gain compensation changes
    if (std::abs(old_gain_comp - gain_comp_in_db) > 0.1f)
    {
        DBG("Gain compensation changed: " + juce::String(old_gain_comp, 2) + " -> " + juce::String(gain_comp_in_db, 2) +
            " dB (Az: " + juce::String(pannerSettings.azimuth, 1) + "°, Div: " + juce::String(pannerSettings.diverge, 1) + "%)");
    }

    if (pannerSettings.isotropicMode)
    {
        if (pannerSettings.equalpowerMode)
        {
            pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::IsotropicEqualPower);
        }
        else
        {
            pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::IsotropicLinear);
        }
    }
    else
    {
        pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::PeriphonicLinear);
    }

    pannerSettings.m1Encode.generatePointResults();
}

void M1PannerAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;

    // Use this method as the place to do any pre-playback
    if (!layoutCreated)
    {
        createLayout(); // this should only be called here after initialization to avoid threading issues
    }

    if (needToUpdateM1EncodePoints)
    {
        updateM1EncodePoints();
        needToUpdateM1EncodePoints = false;
    }

    // this checks if there is a mismatch of expected values set somewhere unexpected and attempts to fix
    if ((int)pannerSettings.m1Encode.getInputMode() != (int)parameters.getParameter(paramInputMode)->convertFrom0to1(parameters.getParameter(paramInputMode)->getValue()))
    {
        DBG("Unexpected Input mismatch! Inputs=" + std::to_string((int)pannerSettings.m1Encode.getInputMode()) + "|" + std::to_string((int)parameters.getParameter(paramInputMode)->convertFrom0to1(parameters.getParameter(paramInputMode)->getValue())));
        auto& params = getValueTreeState();
        auto* param = params.getParameter(paramInputMode);
        param->setValueNotifyingHost(param->convertTo0to1(pannerSettings.m1Encode.getInputMode()));
    }
    if ((int)pannerSettings.m1Encode.getOutputMode() != (int)parameters.getParameter(paramOutputMode)->convertFrom0to1(parameters.getParameter(paramOutputMode)->getValue()))
    {
        DBG("Unexpected Output mismatch! Outputs=" + std::to_string((int)pannerSettings.m1Encode.getOutputMode()) + "|" + std::to_string((int)parameters.getParameter(paramOutputMode)->convertFrom0to1(parameters.getParameter(paramOutputMode)->getValue())));
        auto& params = getValueTreeState();
        auto* param = params.getParameter(paramOutputMode);
        param->setValueNotifyingHost(param->convertTo0to1(pannerSettings.m1Encode.getOutputMode()));
    }

    // Update the host playhead data external usage
    if (external_spatialmixer_active && getPlayHead() != nullptr)
    {
        juce::AudioPlayHead* ph = getPlayHead();
        juce::AudioPlayHead::CurrentPositionInfo currentPlayHeadInfo;
        // Lots of defenses against hosts who do not support playhead data returns
        if (ph->getCurrentPosition(currentPlayHeadInfo))
        {
            hostTimelineData.isPlaying = currentPlayHeadInfo.isPlaying;
            hostTimelineData.playheadPositionInSeconds = currentPlayHeadInfo.timeInSeconds;
        }
    }

    // Set m1Encode obj values for processing
    auto gainCoeffs = pannerSettings.m1Encode.getGains();

    // vector of input channel buffers
    juce::AudioSampleBuffer mainInput = getBusBuffer(buffer, true, 0);
    juce::AudioChannelSet inputLayout = getChannelLayoutOfBus(true, 0);

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
                    // break if expected output channel num size does not match current output channel num size from host

                    // Output channel reordering from fillChannelOrder()
                    int output_channel_reordered = output_channel_indices[output_channel];

                    // process via temp buffer that will also be used for meters
                    if (output_channel_reordered >= 0)
                    {
                        // Get the next Mach1Encode coeff
                        float spatialGainCoeff = smoothedChannelCoeffs[input_channel][output_channel].getNextValue();
                        buf.addSample(output_channel, sample, inValue * spatialGainCoeff);
                    }

                    if (external_spatialmixer_active || mainOutput.getNumChannels() <= 2)
                    { // TODO: check if this doesnt catch too many false cases of hosts not utilizing multichannel output
                        /// ANYTHING REQUIRED ONLY FOR EXTERNAL MIXER GOES HERE
                    }
                    else
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
                            }
#endif // end of ITD_PARAMETERS
                        }
                    }
                }
            }
        }
    }

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

    // update meters
         outputMeterValuedB.resize(mainOutput.getNumChannels()); // expand meter UI number
     for (int output_channel = 0; output_channel < mainOutput.getNumChannels(); output_channel++)
     {
         outputMeterValuedB.set(output_channel, output_channel < mainOutput.getNumChannels() ? juce::Decibels::gainToDecibels(mainOutput.getRMSLevel(output_channel, 0, buffer.getNumSamples())) : -144);
     }
}

void M1PannerAudioProcessor::timerCallback()
{
    // Added if we need to move the OSC stuff from the processorblock
    pannerOSC->update(); // test for connection
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
    auto* editor = new M1PannerAudioProcessorEditor(*this);

    // When the processor sees a new alert, tell the editor to display it
    postAlertToUI = [editor](const Mach1::AlertData& a)
    {
        editor->pannerUIBaseComponent->postAlert(a);
    };

    return editor;
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
        if (!pannerSettings.lockOutputLayout)
        {
            pannerOSC->sendRequestToChangeChannelConfig(pannerSettings.m1Encode.getOutputChannelsCount());
        }
    }
    pannerSettings.m1Encode.setInputMode(inputMode);

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

    needToUpdateM1EncodePoints = true; // need to call to update the m1encode obj for new point counts
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
    // Store the parameters in the memory block.
    juce::MemoryOutputStream stream(destData, false);
    stream.writeString("M1-Panner");

    juce::XmlElement root("Root");
    addXmlElement(root, paramAzimuth, juce::String(pannerSettings.azimuth));
    addXmlElement(root, paramElevation, juce::String(pannerSettings.elevation));
    addXmlElement(root, paramDiverge, juce::String(pannerSettings.diverge));
    addXmlElement(root, paramGain, juce::String(pannerSettings.gain));
    addXmlElement(root, paramStereoOrbitAzimuth, juce::String(pannerSettings.stereoOrbitAzimuth));
    addXmlElement(root, paramStereoSpread, juce::String(pannerSettings.stereoSpread));
    addXmlElement(root, paramStereoInputBalance, juce::String(pannerSettings.stereoInputBalance));
    addXmlElement(root, paramAutoOrbit, juce::String(pannerSettings.autoOrbit ? 1 : 0));
    addXmlElement(root, paramIsotropicEncodeMode, juce::String(pannerSettings.isotropicMode ? 1 : 0));
    addXmlElement(root, paramEqualPowerEncodeMode, juce::String(pannerSettings.equalpowerMode ? 1 : 0));
    addXmlElement(root, paramInputMode, juce::String(pannerSettings.m1Encode.getInputMode()));
    addXmlElement(root, paramOutputMode, juce::String(pannerSettings.m1Encode.getOutputMode()));
#ifdef ITD_PARAMETERS
    addXmlElement(root, paramITDActive, juce::String(pannerSettings.itdActive));
    addXmlElement(root, paramDelayTime, juce::String(pannerSettings.delayTime));
    addXmlElement(root, paramDelayDistance, juce::String(pannerSettings.delayDistance));
#endif

    // Extras
    addXmlElement(root, "trackColor_r", juce::String(osc_colour.red));
    addXmlElement(root, "trackColor_g", juce::String(osc_colour.green));
    addXmlElement(root, "trackColor_b", juce::String(osc_colour.blue));
    addXmlElement(root, "trackColor_a", juce::String(osc_colour.alpha));
    addXmlElement(root, "output_layout_lock", juce::String(pannerSettings.lockOutputLayout ? 1 : 0));

    juce::String strDoc = root.createDocument(juce::String(""), false, false);
    stream.writeString(strDoc);
}

void M1PannerAudioProcessor::setStateInformation(const void* data, int sizeInBytes)
{
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

        // update local parameters first
        parameterChanged(paramAzimuth, (float)getParameterDoubleFromXmlElement(root.get(), paramAzimuth, pannerSettings.azimuth));
        parameterChanged(paramElevation, (float)getParameterDoubleFromXmlElement(root.get(), paramElevation, pannerSettings.elevation));
        parameterChanged(paramDiverge, (float)getParameterDoubleFromXmlElement(root.get(), paramDiverge, pannerSettings.diverge));
        parameterChanged(paramGain, (float)getParameterDoubleFromXmlElement(root.get(), paramGain, pannerSettings.gain));
        parameterChanged(paramStereoOrbitAzimuth, (float)getParameterDoubleFromXmlElement(root.get(), paramStereoOrbitAzimuth, pannerSettings.stereoOrbitAzimuth));
        parameterChanged(paramStereoSpread, (float)getParameterDoubleFromXmlElement(root.get(), paramStereoSpread, pannerSettings.stereoSpread));
        parameterChanged(paramStereoInputBalance, (float)getParameterDoubleFromXmlElement(root.get(), paramStereoInputBalance, pannerSettings.stereoInputBalance));
        parameterChanged(paramAutoOrbit, (int)getParameterIntFromXmlElement(root.get(), paramAutoOrbit, pannerSettings.autoOrbit));
        parameterChanged(paramIsotropicEncodeMode, (int)getParameterIntFromXmlElement(root.get(), paramIsotropicEncodeMode, pannerSettings.isotropicMode));
        parameterChanged(paramEqualPowerEncodeMode, (int)getParameterIntFromXmlElement(root.get(), paramEqualPowerEncodeMode, pannerSettings.equalpowerMode));
        parameterChanged(paramGainCompensationMode, (float)getParameterDoubleFromXmlElement(root.get(), paramGainCompensationMode, pannerSettings.gainCompensationMode));

#ifdef ITD_PARAMETERS
        parameterChanged(paramITDActive, (int)getParameterIntFromXmlElement(root.get(), paramITDActive, pannerSettings.itdActive));
        parameterChanged(paramDelayTime, (int)getParameterIntFromXmlElement(root.get(), paramDelayTime, pannerSettings.delayTime));
        parameterChanged(paramDelayDistance, (float)getParameterDoubleFromXmlElement(root.get(), paramDelayDistance, pannerSettings.delayDistance));
#endif

        // Extras
        osc_colour.red = (int)getParameterIntFromXmlElement(root.get(), "trackColor_r", osc_colour.red);
        osc_colour.green = (int)getParameterIntFromXmlElement(root.get(), "trackColor_g", osc_colour.green);
        osc_colour.blue = (int)getParameterIntFromXmlElement(root.get(), "trackColor_b", osc_colour.blue);
        osc_colour.alpha = (int)getParameterIntFromXmlElement(root.get(), "trackColor_a", osc_colour.alpha);
        parameterChanged("output_layout_lock", (bool)getParameterIntFromXmlElement(root.get(), "output_layout_lock", pannerSettings.lockOutputLayout));

        // if the parsed input from xml is not the default value
        parameterChanged(paramInputMode, Mach1EncodeInputMode(getParameterIntFromXmlElement(root.get(), paramInputMode, pannerSettings.m1Encode.getInputMode())));
        parameterChanged(paramOutputMode, Mach1EncodeOutputMode(getParameterIntFromXmlElement(root.get(), paramOutputMode, pannerSettings.m1Encode.getOutputMode())));
        params.getParameter(paramInputMode)->setValueNotifyingHost(params.getParameter(paramInputMode)->convertTo0to1((int)getParameterIntFromXmlElement(root.get(), paramInputMode, pannerSettings.m1Encode.getInputMode())));
        params.getParameter(paramOutputMode)->setValueNotifyingHost(params.getParameter(paramOutputMode)->convertTo0to1((int)getParameterIntFromXmlElement(root.get(), paramOutputMode, pannerSettings.m1Encode.getOutputMode())));

        // Update all parameters through the value tree
        params.getParameter(paramAzimuth)->setValueNotifyingHost(params.getParameter(paramAzimuth)->convertTo0to1(pannerSettings.azimuth));
        params.getParameter(paramElevation)->setValueNotifyingHost(params.getParameter(paramElevation)->convertTo0to1(pannerSettings.elevation));
        params.getParameter(paramDiverge)->setValueNotifyingHost(params.getParameter(paramDiverge)->convertTo0to1(pannerSettings.diverge));
        params.getParameter(paramGain)->setValueNotifyingHost(params.getParameter(paramGain)->convertTo0to1(pannerSettings.gain));
        params.getParameter(paramStereoOrbitAzimuth)->setValueNotifyingHost(params.getParameter(paramStereoOrbitAzimuth)->convertTo0to1(pannerSettings.stereoOrbitAzimuth));
        params.getParameter(paramStereoSpread)->setValueNotifyingHost(params.getParameter(paramStereoSpread)->convertTo0to1(pannerSettings.stereoSpread));
        params.getParameter(paramStereoInputBalance)->setValueNotifyingHost(params.getParameter(paramStereoInputBalance)->convertTo0to1(pannerSettings.stereoInputBalance));
        params.getParameter(paramAutoOrbit)->setValueNotifyingHost(params.getParameter(paramAutoOrbit)->convertTo0to1(pannerSettings.autoOrbit));
        params.getParameter(paramIsotropicEncodeMode)->setValueNotifyingHost(params.getParameter(paramIsotropicEncodeMode)->convertTo0to1(pannerSettings.isotropicMode));
        params.getParameter(paramEqualPowerEncodeMode)->setValueNotifyingHost(params.getParameter(paramEqualPowerEncodeMode)->convertTo0to1(pannerSettings.equalpowerMode));
        params.getParameter(paramGainCompensationMode)->setValueNotifyingHost(params.getParameter(paramGainCompensationMode)->convertTo0to1(pannerSettings.gainCompensationMode));

#ifdef ITD_PARAMETERS
        params.getParameter(paramITDActive)->setValueNotifyingHost(params.getParameter(paramITDActive)->convertTo0to1(pannerSettings.itdActive));
        params.getParameter(paramDelayTime)->setValueNotifyingHost(params.getParameter(paramDelayTime)->convertTo0to1(pannerSettings.delayTime));
        params.getParameter(paramDelayDistance)->setValueNotifyingHost(params.getParameter(paramDelayDistance)->convertTo0to1(pannerSettings.delayDistance));
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
