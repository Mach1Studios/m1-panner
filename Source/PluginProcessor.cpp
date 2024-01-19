/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

/*
 Architecture:
    - all changes to I/O should be made to pannerSettings first
    - use parameterChanged() with the pannerSettings values
    - parameterChanged() updates the i/o layout
    - parameterChanged() checks if matched with pannerSettings and otherwise updates this too
    parameters expect normalized 0->1 where all the rest of the i/o expects int
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
#ifndef CUSTOM_CHANNEL_LAYOUT
juce::String M1PannerAudioProcessor::paramInputMode("inputMode");
juce::String M1PannerAudioProcessor::paramOutputMode("outputMode");
#endif
#ifdef ITD_PARAMETER
juce::String M1PannerAudioProcessor::paramITDActive("ITDProcessing");
juce::String M1PannerAudioProcessor::paramDelayTime("DelayTime");
juce::String M1PannerAudioProcessor::paramDelayDistance("ITDDistance");
#endif

//==============================================================================
M1PannerAudioProcessor::M1PannerAudioProcessor()
     : AudioProcessor (getHostSpecificLayout()),                 
    parameters(*this, &mUndoManager, juce::Identifier("M1-Panner"),
               {
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramAzimuth, 1),
                                                            TRANS("Azimuth"),
                                                            juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), pannerSettings.azimuth, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramElevation, 1),
                                                            TRANS("Elevation"),
                                                            juce::NormalisableRange<float>(-90.0f, 90.0f, 0.01f), pannerSettings.elevation, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDiverge, 1),
                                                            TRANS("Diverge"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.diverge, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramGain, 1),
                                                            TRANS ("Input Gain"),
                                                            juce::NormalisableRange<float>(-90.0, 24.0f, 0.1f, std::log (0.5f) / std::log (100.0f / 106.0f)),
                                                            pannerSettings.gain, "",
                                                            juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + " dB"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters (3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramAutoOrbit, 1), TRANS("Auto Orbit"), pannerSettings.autoOrbit),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoOrbitAzimuth, 1),
                                                            TRANS("Stereo Orbit Azimuth"),
                                                            juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), pannerSettings.stereoOrbitAzimuth, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoSpread, 1),
                                                            TRANS("Stereo Spread"),
                                                            juce::NormalisableRange<float>(0.0f, 100.0f, 0.01f), pannerSettings.stereoSpread, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramStereoInputBalance, 1),
                                                            TRANS("Stereo Input Balance"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.stereoInputBalance, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramIsotropicEncodeMode, 1), TRANS("Isotropic Encode Mode"), pannerSettings.isotropicMode),
                    std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramEqualPowerEncodeMode, 1), TRANS("Equal Power Encode Mode"), pannerSettings.equalpowerMode),
#ifndef CUSTOM_CHANNEL_LAYOUT
                    std::make_unique<juce::AudioParameterInt>(juce::ParameterID(paramInputMode, 1), TRANS("Input Mode"), 0, Mach1EncodeInputModeBFOAFUMA, Mach1EncodeInputModeMono),
                    std::make_unique<juce::AudioParameterInt>(juce::ParameterID(paramOutputMode, 1), TRANS("Output Mode"), 0, Mach1EncodeOutputModeM1Spatial_60, Mach1EncodeOutputModeM1Spatial_8),
#endif
#ifdef ITD_PARAMETERS
                    std::make_unique<juce::AudioParameterBool>(juce::ParameterID(paramITDActive, 1), TRANS("ITD"), pannerSettings.itdActive),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDelayTime, 1),
                                                            TRANS("Delay Time (max)"),
                                                            juce::NormalisableRange<float>(0.0f, 10000.0f, 1.0f), pannerSettings.delayTime, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "μS"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(1).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(juce::ParameterID(paramDelayDistance, 1),
                                                            TRANS("Delay Distance"),
                                                            juce::NormalisableRange<float>(0.0f, 10000.0f, 0.01f), pannerSettings.delayDistance, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + ""; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(1).getFloatValue(); }),
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
#ifndef CUSTOM_CHANNEL_LAYOUT
    parameters.addParameterListener(paramInputMode, this);
    parameters.addParameterListener(paramOutputMode, this);
#endif
#ifdef ITD_PARAMETERS
    parameters.addParameterListener(paramITDActive, this);
    parameters.addParameterListener(paramDelayTime, this);
    parameters.addParameterListener(paramDelayDistance, this);
#endif
    
    pannerOSC.AddListener([&](juce::OSCMessage msg) {
        DBG("[OSC] Recieved msg | Mode: "+std::to_string(msg[0].getInt32())+", Y: "+std::to_string(msg[1].getFloat32())+", P: "+std::to_string(msg[2].getFloat32()));
        // Capturing monitor mode
        int mode = msg[0].getInt32();
        monitorSettings.monitor_mode = mode;
        
        if (msg.size() >= 2) {
            // Capturing Monitor's Yaw
            if (msg[1].isFloat32()){
                float yaw = msg[1].getFloat32();
                monitorSettings.yaw = yaw; // un-normalised
            }
        }
        if (msg.size() >= 3) {
            // Capturing Monitor's Pitch
            if (msg[2].isFloat32()){
                float pitch = msg[2].getFloat32();
                monitorSettings.pitch = pitch; // un-normalized
            }
        }
    });
    
    // pannerOSC update timer loop
    startTimer(200);
}

M1PannerAudioProcessor::~M1PannerAudioProcessor()
{
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
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int M1PannerAudioProcessor::getCurrentProgram()
{
    return 0;
}

void M1PannerAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String M1PannerAudioProcessor::getProgramName (int index)
{
    return {};
}

void M1PannerAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void M1PannerAudioProcessor::createLayout(){
    int numInputsFromPannerSettings = pannerSettings.m1Encode.getInputChannelsCount();
    int numOutputsFromPannerSettings = pannerSettings.m1Encode.getOutputChannelsCount();
    
    if (external_spatialmixer_active) {
        /// EXTERNAL MULTICHANNEL PROCESSING
        
        // INPUT
        if (pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputModeMono){
            getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::mono());
        }
        else if (pannerSettings.m1Encode.getInputMode() == Mach1EncodeInputModeStereo){
            getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
        }
        // OUTPUT
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
    } else {
        /// INTERNAL MULTICHANNEL PROCESSING
        
        // check if there is a mismatch of the current bus size on PT
        if (hostType.isProTools()) {
            // update the pannerSettings if there is a mismatch
            
            // I/O Concept
            // Inputs: for this plugin we are more literal about inputs, only allowing the number of channels available by host to dictate the input mode
            // Outputs: for this plugin we allow the m1Encode object to have a higher channel count output mode than what the host allows to support more configurations on channel specific hosts

            /// INPUTS
            if (getBus(true, 0)->getCurrentLayout().size() != pannerSettings.m1Encode.getInputMode()) {
                if (getBus(true, 0)->getCurrentLayout().size() == 1) {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeMono);
                } else if (getBus(true, 0)->getCurrentLayout().size() == 2) {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeStereo);
                } else if (getBus(true, 0)->getCurrentLayout().size() == 3) {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeLCR);
                } else if (getBus(true, 0)->getCurrentLayout().size() == 4) {
                    if ((pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputModeQuad) &&
                        (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputModeLCRS) &&
                        (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputModeAFormat) &&
                        (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputModeBFOAACN) &&
                        (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputModeBFOAFUMA)) {
                        // if we are not one of the 4ch formats in pro tools then force default of QUAD
                        pannerSettings.m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeQuad);
                    } else {
                        // already set
                    }
                } else if (getBus(true, 0)->getCurrentLayout().size() == 5) {
                    pannerSettings.m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputMode5dot0);
                } else if (getBus(true, 0)->getCurrentLayout().size() == 6) {
                    if ((pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputMode5dot1Film) &&
                        (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputMode5dot1DTS) &&
                        (pannerSettings.m1Encode.getInputMode() != Mach1EncodeInputModeType::Mach1EncodeInputMode5dot1SMTPE)) {
                        // if we are not one of the 4ch formats in pro tools then force default of 5.1 film
                        pannerSettings.m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputMode5dot1Film);
                    } else {
                        // already set
                    }
                } else {
                    // an unsupported format
                }
            }
            // update parameter from start
            parameters.getParameter(paramInputMode)->setValue(parameters.getParameter(paramInputMode)->convertTo0to1(pannerSettings.m1Encode.getInputMode()));

            /// OUTPUTS
            if (getBus(false, 0)->getCurrentLayout().size() != pannerSettings.m1Encode.getOutputMode()) {
                if (getBus(false, 0)->getCurrentLayout().size() == 4) {
                    pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Horizon_4);
                } else if (getBus(false, 0)->getCurrentLayout().size() == 8) {
                    pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_8);
                } else if (getBus(false, 0)->getCurrentLayout().size() == 16) {
                    if ((pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Horizon_4) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_8) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_12) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_14)) {
                        pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_14);
                    }
                } else if (getBus(false, 0)->getCurrentLayout().size() == 36) {
                    if ((pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Horizon_4) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_8) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_12) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_14) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_32) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_36)) {
                        pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_36);
                    }
                } else if (getBus(false, 0)->getCurrentLayout().size() == 64) {
                    if ((pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Horizon_4) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_8) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_12) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_14) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_32) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_36) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_48) &&
                        (pannerSettings.m1Encode.getOutputMode() != Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_60)) {
                        pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_60);
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
void M1PannerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    if (!layoutCreated) {
        createLayout();
    }

    // can still be used to calculate coeffs even in STREAMING_PANNER_PLUGIN mode
    processorSampleRate = sampleRate;
    
    if (pannerSettings.m1Encode.getOutputChannelsCount() != getMainBusNumOutputChannels()){
        bool channel_io_error = -1;
        // error handling here?
    }
    
    // Checks if output bus is non DISCRETE layout and fixes host specific channel ordering issues
    fillChannelOrderArray(pannerSettings.m1Encode.getOutputChannelsCount());
    
#ifdef ITD_PARAMETERS
    mSampleRate = sampleRate;
    mDelayTimeSmoother.reset(samplesPerBlock);
    mDelayTimeSmoother.setValue(0);
    
    ring.reset(new RingBuffer(pannerSettings..getOutputChannelsCount(), 64*sampleRate));
    ring->clear();

    // sample buffer for 2 seconds + MAX_NUM_CHANNELS buffers safety
    mDelayBuffer.setSize (pannerSettings..getOutputChannelsCount(), (float)pannerSettings.m1Encode.getOutputChannelsCount() * (samplesPerBlock + sampleRate), false, false);
    mDelayBuffer.clear();
    mExpectedReadPos = -1;
#endif
}

void M1PannerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void M1PannerAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    if (parameterID == paramAzimuth) {
        parameters.getParameter(paramAzimuth)->setValue(newValue);
    } else if (parameterID == paramElevation) {
        parameters.getParameter(paramElevation)->setValue(newValue);
    } else if (parameterID == paramDiverge) {
        parameters.getParameter(paramDiverge)->setValue(newValue);
    } else if (parameterID == paramGain) {
        parameters.getParameter(paramGain)->setValue(newValue);
    } else if (parameterID == paramAutoOrbit) {
        parameters.getParameter(paramAutoOrbit)->setValue(newValue);
    } else if (parameterID == paramStereoOrbitAzimuth) {
        parameters.getParameter(paramStereoOrbitAzimuth)->setValue(newValue);
    } else if (parameterID == paramStereoSpread) {
        parameters.getParameter(paramStereoSpread)->setValue(newValue);
    } else if (parameterID == paramStereoInputBalance) {
        parameters.getParameter(paramStereoInputBalance)->setValue(newValue);
    } else if (parameterID == paramIsotropicEncodeMode) {
        parameters.getParameter(paramIsotropicEncodeMode)->setValue(newValue);
        // set in UI
    } else if (parameterID == paramEqualPowerEncodeMode) {
        parameters.getParameter(paramEqualPowerEncodeMode)->setValue(newValue);
        // set in UI
    } else if (parameterID == paramInputMode) {
        // stop pro tools from using plugin data to change input after creation
        if (!hostType.isProTools() || (hostType.isProTools() && (getTotalNumInputChannels() == 4 || getTotalNumInputChannels() == 6))) {
            juce::RangedAudioParameter* parameterInputMode = parameters.getParameter(paramInputMode);
            parameterInputMode->setValue(parameterInputMode->convertTo0to1(newValue));
            Mach1EncodeInputModeType inputType = Mach1EncodeInputModeType((int)newValue);
            layoutCreated = false;
            createLayout();
        }
    } else if (parameterID == paramOutputMode) {
        // stop pro tools from using plugin data to change output after creation
        if (!hostType.isProTools() || (hostType.isProTools() && getTotalNumOutputChannels() > 8)) {
            juce::RangedAudioParameter* parameterOutputMode = parameters.getParameter(paramOutputMode);
            parameterOutputMode->setValue(parameterOutputMode->convertTo0to1(newValue));
            Mach1EncodeOutputModeType outputType = Mach1EncodeOutputModeType((int)newValue);
            layoutCreated = false;
            createLayout();
        }
    }
}

#ifndef CUSTOM_CHANNEL_LAYOUT
bool M1PannerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    juce::PluginHostType hostType;
    Mach1Encode configTester;
    
    // block plugin if input or output is disabled on construction
    if (layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled()
     || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;

    if (hostType.isReaper()) {
        return true;
    }
    
    if (hostType.isProTools()) {
        if ((   layouts.getMainInputChannelSet().size() == juce::AudioChannelSet::mono().size()
            ||  layouts.getMainInputChannelSet().size() == juce::AudioChannelSet::stereo().size()
            ||  layouts.getMainInputChannelSet() == juce::AudioChannelSet::createLCR()
            ||  layouts.getMainInputChannelSet().size() == juce::AudioChannelSet::quadraphonic().size()
            ||  layouts.getMainInputChannelSet() == juce::AudioChannelSet::create5point0()
            ||  layouts.getMainInputChannelSet() == juce::AudioChannelSet::create5point1()
            ||  layouts.getMainInputChannelSet() == juce::AudioChannelSet::create6point0())
            //||  layouts.getMainInputChannelSet() == AudioChannelSet::ambisonic(2)
            //||  layouts.getMainInputChannelSet() == AudioChannelSet::ambisonic(3)
            &&
            (   layouts.getMainOutputChannelSet() == juce::AudioChannelSet::create7point1()
             || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::quadraphonic()
             || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(3)
             || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(5)
             || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::ambisonic(7)
            ) ) {
                return true;
        } else {
            return false;
        }
    } else if ((layouts.getMainInputChannelSet() == juce::AudioChannelSet::mono() || layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo()) && (layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo())) {
        // RETURN TRUE FOR EXTERNAL STREAMING MODE
        // hard set {1,2} and {2,2} for streaming use case
        return true;
    } else {
        // Test for all available Mach1Encode configs
        // manually maintained for-loop of first enum element to last enum element
        for (int inputEnum = Mach1EncodeInputModeMono; inputEnum != Mach1EncodeInputMode5dot1SMTPE; inputEnum++ ) {
            configTester.setInputMode(static_cast<Mach1EncodeInputModeType>(inputEnum));
            // test each input, if the input has the number of channels as the input testing layout has move on to output testing
            if (layouts.getMainInputChannelSet().size() == configTester.getInputChannelsCount()) {
                for (int outputEnum = 0; outputEnum != Mach1EncodeOutputModeM1Spatial_60; outputEnum++ ) {
                    // test each output
                   configTester.setOutputMode(static_cast<Mach1EncodeOutputModeType>(outputEnum));
                    if (layouts.getMainOutputChannelSet().size() == configTester.getOutputChannelsCount()){
                        return true;
                    }
                }
            }
        }
        return false;
    }
}
#endif

void M1PannerAudioProcessor::fillChannelOrderArray(int numOutputChannels) {
    orderOfChans.resize(numOutputChannels);
    output_channel_indices.resize(numOutputChannels);
    
    // get output layout
    juce::AudioChannelSet chanset = getBus(false, 0)->getCurrentLayout();
    
    if(!chanset.isDiscreteLayout() && numOutputChannels == 8) {
        // Layout for Pro Tools
        if (hostType.isProTools()){
            // TODO: expand this for other surround configs on PT
            if (chanset.getDescription().contains(juce::String("7.1 Surround"))) {
                // In PT ensure we are on a 7.1 bus
                orderOfChans[0] = juce::AudioChannelSet::ChannelType::left;
                orderOfChans[1] = juce::AudioChannelSet::ChannelType::centre;
                orderOfChans[2] = juce::AudioChannelSet::ChannelType::right;
                orderOfChans[3] = juce::AudioChannelSet::ChannelType::leftSurroundSide;
                orderOfChans[4] = juce::AudioChannelSet::ChannelType::rightSurroundSide;
                orderOfChans[5] = juce::AudioChannelSet::ChannelType::leftSurroundRear;
                orderOfChans[6] = juce::AudioChannelSet::ChannelType::rightSurroundRear;
                orderOfChans[7] = juce::AudioChannelSet::ChannelType::LFE;
            } else {
                // set the order index for plugins instantiated on buses that are higher channel counts than the encode is currently set to
                for (int i = 0; i < numOutputChannels; i ++) {
                    orderOfChans[i] = chanset.getTypeOfChannel(i);
                }
            }
        } else {
            orderOfChans[0] = juce::AudioChannelSet::ChannelType::left;
            orderOfChans[1] = juce::AudioChannelSet::ChannelType::right;
            orderOfChans[2] = juce::AudioChannelSet::ChannelType::centre;
            orderOfChans[3] = juce::AudioChannelSet::ChannelType::LFE;
            orderOfChans[4] = juce::AudioChannelSet::ChannelType::leftSurroundSide;
            orderOfChans[5] = juce::AudioChannelSet::ChannelType::rightSurroundSide;
            orderOfChans[6] = juce::AudioChannelSet::ChannelType::leftSurroundRear;
            orderOfChans[7] = juce::AudioChannelSet::ChannelType::rightSurroundRear;
        }
        if (chanset.size() >= 8) {
            for (int i = 0; i < numOutputChannels; i ++) {
                output_channel_indices[i] = chanset.getChannelIndexForType(orderOfChans[i]);
            }
        }
    } else if (!chanset.isDiscreteLayout() && numOutputChannels == 4){
        // Layout for Pro Tools
        if (hostType.isProTools()) {
            orderOfChans[0] = juce::AudioChannelSet::ChannelType::left;
            orderOfChans[1] = juce::AudioChannelSet::ChannelType::right;
            orderOfChans[2] = juce::AudioChannelSet::ChannelType::rightSurround;
            orderOfChans[3] = juce::AudioChannelSet::ChannelType::leftSurround;
        } else {
            orderOfChans[0] = juce::AudioChannelSet::ChannelType::left;
            orderOfChans[1] = juce::AudioChannelSet::ChannelType::right;
            orderOfChans[2] = juce::AudioChannelSet::ChannelType::leftSurround;
            orderOfChans[3] = juce::AudioChannelSet::ChannelType::rightSurround;
        }
        if (chanset.size() >= 4) {
            for (int i = 0; i < numOutputChannels; i ++) {
                output_channel_indices[i] = chanset.getChannelIndexForType(orderOfChans[i]);
            }
        }
    } else {
        for (int i = 0; i < numOutputChannels; ++i){
            orderOfChans[i] = juce::AudioChannelSet::ChannelType::discreteChannel0;
            output_channel_indices[i] = i;
        }
    }
}

void M1PannerAudioProcessor::updateM1EncodePoints() {
    float _diverge = pannerSettings.diverge;
    float _gain = pannerSettings.gain;
    
    if (monitorSettings.monitor_mode == 1) { // StereoSafe mode is on
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
    
    pannerSettings.m1Encode.setAutoOrbit(pannerSettings.autoOrbit);
    pannerSettings.m1Encode.setOrbitRotationDegrees(pannerSettings.stereoOrbitAzimuth);
    pannerSettings.m1Encode.setStereoSpread(pannerSettings.stereoSpread/100.0); // Mach1Encode expects an unsigned normalized input
    
    if (pannerSettings.isotropicMode) {
        if (pannerSettings.equalpowerMode) {
            pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::Mach1EncodePannerModeIsotropicEqualPower);
        } else {
            pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::Mach1EncodePannerModeIsotropicLinear);
        }
    } else {
        pannerSettings.m1Encode.setPannerMode(Mach1EncodePannerMode::Mach1EncodePannerModePeriphonicLinear);
    }
    
    pannerSettings.m1Encode.generatePointResults();
}

void M1PannerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Use this method as the place to do any pre-playback
    if (!layoutCreated) {
        createLayout();
    }
    
    // this checks if there is a mismatch of expected values set somewhere unexpected and attempts to fix
	if ((int)pannerSettings.m1Encode.getInputMode() != (int)parameters.getParameter(paramInputMode)->convertFrom0to1(parameters.getParameter(paramInputMode)->getValue()) || (int)pannerSettings.m1Encode.getOutputMode() != (int)parameters.getParameter(paramOutputMode)->convertFrom0to1(parameters.getParameter(paramOutputMode)->getValue())) {
        DBG("Unexpected Input/Output mismatch! Inputs="+std::to_string((int)pannerSettings.m1Encode.getInputMode())+"|"+std::to_string((int)parameters.getParameter(paramInputMode)->convertFrom0to1(parameters.getParameter(paramInputMode)->getValue())) + ", Outputs="+std::to_string((int)pannerSettings.m1Encode.getOutputMode())+"|"+std::to_string((int)parameters.getParameter(paramOutputMode)->convertFrom0to1(parameters.getParameter(paramOutputMode)->getValue())));
        parameterChanged(paramInputMode, pannerSettings.m1Encode.getInputMode());
        parameterChanged(paramOutputMode, pannerSettings.m1Encode.getOutputMode());
	}
    
    // Update the host playhead data external usage
    if (external_spatialmixer_active && getPlayHead() != nullptr) {
        juce::AudioPlayHead* ph = getPlayHead();
        juce::AudioPlayHead::CurrentPositionInfo currentPlayHeadInfo;
        // Lots of defenses against hosts who do not support playhead data returns
        if (ph->getCurrentPosition(currentPlayHeadInfo)) {
            hostTimelineData.isPlaying = currentPlayHeadInfo.isPlaying;
            hostTimelineData.playheadPositionInSeconds = currentPlayHeadInfo.timeInSeconds;
        }
    }
    
    // Set m1Encode obj values for processing
    updateM1EncodePoints();
    auto gainCoeffs = pannerSettings.m1Encode.getGains();

    // vector of input channel buffers
    juce::AudioSampleBuffer mainInput = getBusBuffer(buffer, true, 0);
    juce::AudioChannelSet inputLayout = getChannelLayoutOfBus(true, 0);
    
    // output buffers
    juce::AudioSampleBuffer mainOutput = getBusBuffer(buffer, false, 0);

#ifdef ITD_PARAMETERS
    mDelayTimeSmoother.setTargetValue(delayTimeParameter->get());
    const float udtime = mDelayTimeSmoother.getNextValue() * mSampleRate/1000000; // number of samples in a microsecond * number of microseconds
#endif
    
    // Multiply input buffer by inputGain parameter
    mainInput.applyGain(juce::Decibels::decibelsToGain(pannerSettings.gain));
    
    // input pan balance for stereo input
    if (mainInput.getNumChannels() > 1 && pannerSettings.m1Encode.getInputChannelsCount() == 2) {
        float p = juce::MathConstants<float>::pi * (pannerSettings.stereoInputBalance + 1)/4;
        mainInput.applyGain(0, 0, buffer.getNumSamples(), std::cos(p)); // gain for Left
        mainInput.applyGain(1, 0, buffer.getNumSamples(), std::sin(p)); // gain for Right
    }
    
    // resize the processing buffer and zero it out so it works despite how many of the expected channels actually exist host side
    audioDataIn.resize(mainInput.getNumChannels()); // resizing the process data to what the host can support
    // TODO: error handle for when requested m1Encode input size is more than the host supports
    for (int input_channel = 0; input_channel < mainInput.getNumChannels(); input_channel++){
        audioDataIn[input_channel].resize(buffer.getNumSamples(), 0.0);
    }

    // input channel setup loop
    for (int input_channel = 0; input_channel < pannerSettings.m1Encode.getInputChannelsCount(); input_channel++){
        if (input_channel > mainInput.getNumChannels()-1) {
            // TODO: error?
            break;
        } else {
            // Copy input data to additional buffer
            memcpy(audioDataIn[input_channel].data(), mainInput.getReadPointer(input_channel), sizeof(float) * buffer.getNumSamples());

            // output channel setup loop
            for (int output_channel = 0; output_channel < pannerSettings.m1Encode.getOutputChannelsCount(); output_channel++){
                // We apply a channel re-ordering for DAW canonical specific output channel configrations via fillChannelOrder() and `output_channel_reordered`
                // Output channel reordering from fillChannelOrder()
                int output_channel_reordered = output_channel_indices[output_channel];
                smoothedChannelCoeffs[input_channel][output_channel_reordered].setTargetValue(gainCoeffs[input_channel][output_channel_reordered]);
            }
        }
    }

    // multichannel temp buffer (also used for informing meters even when not processing to write pointers
    // Note: Use buf.getNumChannels() for output size from this point on to not mismatch from new m1Encode size requests
    juce::AudioBuffer<float> buf(pannerSettings.m1Encode.getOutputChannelsCount(), buffer.getNumSamples());
    buf.clear();
    // multichannel output buffer (if internal processing is active this will have the above copy into it)
    float* const* outBuffer = mainOutput.getArrayOfWritePointers();

    // prepare the output buffer
    for (int output_channel = 0; output_channel < buf.getNumChannels(); output_channel++) {
        // break if expected output channel num size does not match current output channel num size from host
        if (output_channel > mainOutput.getNumChannels()-1) {
            // TODO: Test for external_mixer because we are not seeing the expected multichannel output?
            break;
        } else {
            // clear the output buffer
            for (int sample = 0; sample < buffer.getNumSamples(); sample++) {
                outBuffer[output_channel][sample] = 0;
            }
        }
    }
    
    // processing loop
    for (int input_channel = 0; input_channel < pannerSettings.m1Encode.getInputChannelsCount(); input_channel++){
        for (int sample = 0; sample < buffer.getNumSamples(); sample++){
            // break if expected input channel num size does not match current input channel num size from host
            if (input_channel > mainInput.getNumChannels()-1) {
                break;
            } else {
                // Get each input sample per channel
                float inValue = audioDataIn[input_channel][sample];
                
                // Apply to each of the output channels per input channel
                for (int output_channel = 0; output_channel < buf.getNumChannels(); output_channel++){
                    // break if expected output channel num size does not match current output channel num size from host
                    
                    // Get the next Mach1Encode coeff
                    float spatialGainCoeff = smoothedChannelCoeffs[input_channel][output_channel].getNextValue();

                    // TODO: check if `output_channel_reordered` is appropriate here
                    // Output channel reordering from fillChannelOrder()
                    int output_channel_reordered = output_channel_indices[output_channel];
                    
                    // process via temp buffer that will also be used for meters
                    buf.addSample(output_channel_reordered, sample, inValue * spatialGainCoeff);

                    if (external_spatialmixer_active || mainOutput.getNumChannels() <= 2) { // TODO: check if this doesnt catch too many false cases of hosts not utilizing multichannel output
                        /// ANYTHING REQUIRED ONLY FOR EXTERNAL MIXER GOES HERE
                        
                    } else {
                        /// ANYTHING THAT IS ONLY FOR INTERNAL MULTICHANNEL PROCESSING GOES HERE
                        if (output_channel > mainOutput.getNumChannels()-1) {
                            // TODO: Test for external_mixer?
                            break;
                        } else {
                            // apply processed output samples to become the output buffers
                            outBuffer[output_channel_reordered][sample] += buf.getSample(output_channel_reordered, sample);
                            
                            #ifdef ITD_PARAMETERS
                            //SIMPLE DELAY
                            // scale delayCoeffs to be normalized
                            for (int i = 0; i < pannerSettings.m1Encode.getInputChannelsCount(); i++) {
                                for (int o = 0; o < pannerSettings.m1Encode.getOutputChannelsCount(); o++) {
                                    delayCoeffs[i][o] = std::min(0.25f, delayCoeffs[i][o]); // clamp maximum to .25f
                                    delayCoeffs[i][o] *= 4.0f; // rescale range to 0.0->1.0
                                    // Incorporate the distance delay multiplier
                                    // using min to correlate delayCoeffs as multiplier increases
                                    //delayCoeffs[i][o] = std::min<float>(1.0f, (delayCoeffs[i][o]+0.01f) * (float)delayDistanceParameter->get()/100.);
                                    //delayCoeffs[i][o] *= delayDistanceParameter->get()/10.;
                                }
                            }
                            
                            if ((bool)*itdParameter) {
                                for (int sample = 0; sample < numSamples; sample++) {
                                    // write original to delay
                                    float udtime = mDelayTimeSmoother.getNextValue() * mSampleRate / 1000000; // number of samples in a microsecond * number of microseconds
                                    for (auto channel = 0; channel < pannerSettings.m1Encode.getOutputChannelsCount(); channel++) {
                                        ring->pushSample(channel, outBuffer[channel][sample]);
                                    }
                                    for (int channel = 0; channel < pannerSettings.m1Encode.getOutputChannelsCount(); channel++) {
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
    
    // update meters
    outputMeterValuedB.resize(buf.getNumChannels()); // expand meter UI number
    for (int output_channel = 0; output_channel < buf.getNumChannels(); output_channel++) {
        outputMeterValuedB.set(output_channel, output_channel < buf.getNumChannels() ? juce::Decibels::gainToDecibels(buf.getRMSLevel(output_channel, 0, buf.getNumSamples())) : -144 );
    }

}

void M1PannerAudioProcessor::timerCallback() {
    // Added if we need to move the OSC stuff from the processorblock
    pannerOSC.update(); // test for connection
    if (pannerOSC.IsConnected()) {
        pannerOSC.sendPannerSettings((int)pannerSettings.m1Encode.getInputMode(), pannerSettings.azimuth, pannerSettings.elevation, pannerSettings.diverge, pannerSettings.gain);
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
    return new M1PannerAudioProcessorEditor (*this);
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
    if (xml->getChildByName("param_" + paramName) && xml->getChildByName("param_" + paramName)->hasAttribute("value")) {
        double val = xml->getChildByName("param_" + paramName)->getDoubleAttribute("value", defVal);
        if (std::isnan(val)) {
            return defVal;
        }
        return val;
    }
    return defVal;
}

int getParameterIntFromXmlElement(juce::XmlElement* xml, juce::String paramName, int defVal)
{
    if (xml->getChildByName("param_" + paramName) && xml->getChildByName("param_" + paramName)->hasAttribute("value")) {
        return xml->getChildByName("param_" + paramName)->getDoubleAttribute("value", defVal);
    }
    return defVal;
}

void M1PannerAudioProcessor::m1EncodeChangeInputOutputMode(Mach1EncodeInputModeType inputMode, Mach1EncodeOutputModeType outputMode) {
    pannerSettings.m1Encode.setInputMode(inputMode);
    pannerSettings.m1Encode.setOutputMode(outputMode);

    auto inputChannelsCount = pannerSettings.m1Encode.getInputChannelsCount();
    auto outputChannelsCount = pannerSettings.m1Encode.getOutputChannelsCount();

    smoothedChannelCoeffs.resize(inputChannelsCount);
    orderOfChans.resize(outputChannelsCount);
    output_channel_indices.resize(outputChannelsCount);

    // Checks if output bus is non DISCRETE layout and fixes host specific channel ordering issues
    fillChannelOrderArray(outputChannelsCount);
    
    for (int input_channel = 0; input_channel < inputChannelsCount; input_channel++) {
        smoothedChannelCoeffs[input_channel] = std::vector<juce::LinearSmoothedValue<float>>();
        smoothedChannelCoeffs[input_channel].resize(outputChannelsCount);
        for (int output_channel = 0; output_channel < pannerSettings.m1Encode.getOutputChannelsCount(); output_channel++) {
            smoothedChannelCoeffs[input_channel][output_channel].reset(processorSampleRate, (double)0.01);
        }
    }
}

void M1PannerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    // You should use this method to store your parameters in the memory block.
    juce::MemoryOutputStream stream(destData, false);
    // DO NOT CHANGE THIS NUMBER, it is not a version tracker but a version tag threshold for supporting
    // backwards compatible automation data in PT
    stream.writeString("2.0.0"); // write the last major prefix
    
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

    juce::String strDoc = root.createDocument(juce::String(""), false, false);
    stream.writeString(strDoc);
}

void M1PannerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::MemoryInputStream input(data, sizeInBytes, false);
    auto prefix = input.readString();
    /*
     This prefix string check is to define when we swap from mState parameters to newer AVPTS, using this to check if the plugin
     was made before this release version (1.5.1) since it would still be using mState, if it is a M1-Panner made before "1.5.1"
     then we use the mState tree to call the saved automation and values of all our parameters in those older sessions.

     WARNING: Do not update this string value unless there is a specific reason, it is not designed to be updated with each
     release version update.
    */

    if (!prefix.isEmpty() && (prefix == "1.5.1" || prefix == "2.0.0")) {
        juce::XmlDocument doc(input.readString());
        std::unique_ptr<juce::XmlElement> root(doc.getDocumentElement());
    
        pannerSettings.azimuth = getParameterDoubleFromXmlElement(root.get(), paramAzimuth, pannerSettings.azimuth);
        pannerSettings.elevation = getParameterDoubleFromXmlElement(root.get(), paramElevation, pannerSettings.elevation);
        pannerSettings.diverge = getParameterDoubleFromXmlElement(root.get(), paramDiverge, pannerSettings.diverge);
        pannerSettings.gain = getParameterDoubleFromXmlElement(root.get(), paramGain, pannerSettings.gain);
        pannerSettings.stereoOrbitAzimuth = getParameterDoubleFromXmlElement(root.get(), paramStereoOrbitAzimuth, pannerSettings.stereoOrbitAzimuth);
        pannerSettings.stereoSpread = getParameterDoubleFromXmlElement(root.get(), paramStereoSpread, pannerSettings.stereoSpread);
        pannerSettings.stereoInputBalance = getParameterDoubleFromXmlElement(root.get(), paramStereoInputBalance, pannerSettings.stereoInputBalance);
        pannerSettings.autoOrbit = getParameterIntFromXmlElement(root.get(), paramAutoOrbit, pannerSettings.autoOrbit);
        pannerSettings.isotropicMode = getParameterIntFromXmlElement(root.get(), paramIsotropicEncodeMode, pannerSettings.isotropicMode);
        pannerSettings.equalpowerMode = getParameterIntFromXmlElement(root.get(), paramEqualPowerEncodeMode, pannerSettings.equalpowerMode);

#ifdef ITD_PARAMETERS
        pannerSettings.itdActive = getParameterIntFromXmlElement(root.get(), paramITDActive, pannerSettings.itdActive);
        pannerSettings.delayTime = getParameterIntFromXmlElement(root.get(), paramDelayTime, pannerSettings.delayTime);
        pannerSettings.delayDistance = getParameterDoubleFromXmlElement(root.get(), paramDelayDistance, pannerSettings.delayDistance);
#endif
    
// Parsing old plugin QUADMODE and applying to new inputType structure
        if (prefix == "1.5.1") {
            // Get QUAD input type
            int quadStoredInput = (int)getParameterIntFromXmlElement(root.get(), paramInputMode, pannerSettings.m1Encode.getInputMode());
            Mach1EncodeInputModeType tempInputType;
            if (quadStoredInput == 0) {
                tempInputType = Mach1EncodeInputModeQuad; // 3
            } else if (quadStoredInput == 1) {
                tempInputType = Mach1EncodeInputModeLCRS; // 4
            } else if (quadStoredInput == 2) {
                tempInputType = Mach1EncodeInputModeAFormat; // 5
            } else if (quadStoredInput == 3) {
                tempInputType = Mach1EncodeInputModeBFOAACN; // 10
            } else if (quadStoredInput == 4) {
                tempInputType = Mach1EncodeInputModeBFOAFUMA; // 11
            } else {
                // error
            }
            pannerSettings.m1Encode.setInputMode(tempInputType);
            parameterChanged(paramInputMode, tempInputType);
        } else if (prefix == "2.0.0") {
            // if the parsed input from xml is not the default value
            if (getParameterIntFromXmlElement(root.get(), paramInputMode, pannerSettings.m1Encode.getInputMode()) != (int)pannerSettings.m1Encode.getInputMode()) {
                pannerSettings.m1Encode.setInputMode(Mach1EncodeInputModeType(getParameterIntFromXmlElement(root.get(), paramInputMode, pannerSettings.m1Encode.getInputMode())));
                parameterChanged(paramInputMode, Mach1EncodeInputModeType(getParameterIntFromXmlElement(root.get(), paramInputMode, pannerSettings.m1Encode.getInputMode())));
            }
            // if the parsed output from xml is not the default value
            if (getParameterIntFromXmlElement(root.get(), paramOutputMode, pannerSettings.m1Encode.getInputMode()) != (int)pannerSettings.m1Encode.getOutputMode()) {
                pannerSettings.m1Encode.setOutputMode(Mach1EncodeOutputModeType(getParameterIntFromXmlElement(root.get(), paramOutputMode, pannerSettings.m1Encode.getInputMode())));
                parameterChanged(paramOutputMode, Mach1EncodeOutputModeType(getParameterIntFromXmlElement(root.get(), paramOutputMode, pannerSettings.m1Encode.getInputMode())));
            }
        }
    } else {
        // Legacy recall
        //legacyParametersRecall(data, sizeInBytes, *this);
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new M1PannerAudioProcessor();
}
