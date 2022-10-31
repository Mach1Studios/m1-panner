/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

juce::String M1PannerAudioProcessor::paramAzimuth("azimuth");
juce::String M1PannerAudioProcessor::paramElevation("elevation"); // also Z
juce::String M1PannerAudioProcessor::paramDiverge("diverge");
juce::String M1PannerAudioProcessor::paramGain("gain");
juce::String M1PannerAudioProcessor::paramX("x");
juce::String M1PannerAudioProcessor::paramY("y");
juce::String M1PannerAudioProcessor::paramStereoOrbitAzimuth("orbitAzimuth");
juce::String M1PannerAudioProcessor::paramStereoSpread("orbitSpread");
juce::String M1PannerAudioProcessor::paramStereoInputBalance("orbitBalance");
juce::String M1PannerAudioProcessor::paramAutoOrbit("autoOrbit");
juce::String M1PannerAudioProcessor::paramIsotropicEncodeMode("isotropicEncodeMode");
juce::String M1PannerAudioProcessor::paramEqualPowerEncodeMode("equalPowerEncodeMode");
#if defined(DYNAMIC_IO_PLUGIN_MODE) || defined(STREAMING_PANNER_PLUGIN)
juce::String M1PannerAudioProcessor::paramInputMode("inputMode");
juce::String M1PannerAudioProcessor::paramOutputMode("outputMode");
#endif
#ifdef ITD_PARAMETER
juce::String M1PannerAudioProcessor::paramITDActive("ITDProcessing");
juce::String M1PannerAudioProcessor::paramDelayTime("DelayTime");
juce::String M1PannerAudioProcessor::paramITDClampActive("ITDClamp");
juce::String M1PannerAudioProcessor::paramDelayDistance("ITDDistance");
#endif

//==============================================================================
M1PannerAudioProcessor::M1PannerAudioProcessor()
     : AudioProcessor (BusesProperties()
            #ifdef CUSTOM_CHANNEL_LAYOUT
                .withInput("Input", juce::AudioChannelSet::discreteChannels(INPUT_CHANNELS), true)
            #elif defined(STREAMING_PANNER_PLUGIN)
                .withInput("Input", juce::AudioChannelSet::stereo(), true)
            #else
                #if (JucePlugin_Build_AAX == 1 || JucePlugin_Build_RTAS == 1)
                    .withInput("Default Input", juce::AudioChannelSet::stereo(), true)
                #else
                    .withInput("Default Input", juce::AudioChannelSet::stereo(), true)
                #endif
            #endif
                    //removing this to solve mono/stereo plugin build issue on VST/AU/VST3
                    //.withInput("Side Chain Mono", juce::AudioChannelSet::mono(), true)
            #ifdef CUSTOM_CHANNEL_LAYOUT
                .withOutput("Mach1 Out", juce::AudioChannelSet::discreteChannels(OUTPUT_CHANNELS), true)),
            #elif defined(STREAMING_PANNER_PLUGIN)
                .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
            #else
                #if (JucePlugin_Build_AAX == 1 || JucePlugin_Build_RTAS == 1)
                    .withOutput("Default Output", juce::AudioChannelSet::create7point1(), true)),
                #else
                    .withOutput ("Mach1 Out 1", juce::AudioChannelSet::mono(), true)
                    .withOutput ("Mach1 Out 2", juce::AudioChannelSet::mono(), true)
                    .withOutput ("Mach1 Out 3", juce::AudioChannelSet::mono(), true)
                    .withOutput ("Mach1 Out 4", juce::AudioChannelSet::mono(), true)
                    .withOutput ("Mach1 Out 5", juce::AudioChannelSet::mono(), true)
                    .withOutput ("Mach1 Out 6", juce::AudioChannelSet::mono(), true)
                    .withOutput ("Mach1 Out 7", juce::AudioChannelSet::mono(), true)
                    .withOutput ("Mach1 Out 8", juce::AudioChannelSet::mono(), true)),
                #endif
            #endif
    parameters(*this, &mUndoManager, juce::Identifier("M1-Panner"),
               {
                    std::make_unique<juce::AudioParameterFloat>(paramAzimuth,
                                                            TRANS("Azimuth"),
                                                            juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), pannerSettings.azimuth, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramElevation,
                                                            TRANS("Elevation"),
                                                            juce::NormalisableRange<float>(-90.0f, 90.0f, 0.01f), pannerSettings.elevation, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramDiverge,
                                                            TRANS("Diverge"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.diverge, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramX,
                                                            TRANS("X"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.x, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramY,
                                                            TRANS("Y"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.y, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramGain,
                                                            TRANS ("Input Gain"),
                                                    juce::NormalisableRange<float>(-100.0f, 6.0f, 0.1f, std::log (0.5f) / std::log (100.0f / 106.0f)),
                                                            pannerSettings.gain, "",
                                                    juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + " dB"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters (3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterBool>(paramAutoOrbit, TRANS("Auto Orbit"), pannerSettings.autoOrbit),
                    std::make_unique<juce::AudioParameterFloat>(paramStereoOrbitAzimuth,
                                                            TRANS("Stereo Orbit Rotation"),
                                                            juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), pannerSettings.stereoOrbitAzimuth, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramStereoSpread,
                                                            TRANS("Stereo Spread"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.stereoSpread, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramStereoInputBalance,
                                                            TRANS("Stereo Input Balance"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), pannerSettings.stereoInputBalance, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterBool>(paramIsotropicEncodeMode, TRANS("Isotropic Encode Mode"), pannerSettings.isotropicMode),
                    std::make_unique<juce::AudioParameterBool>(paramEqualPowerEncodeMode, TRANS("Equal Power Encode Mode"), pannerSettings.equalpowerMode),
#if defined(DYNAMIC_IO_PLUGIN_MODE) || defined(STREAMING_PANNER_PLUGIN)
                    // Limited to stereo input for STREAMING_PANNER_PLUGIN mode
                    std::make_unique<juce::AudioParameterInt>(paramInputMode, TRANS("Input Mode"), 0, Mach1EncodeInputModeStereo, Mach1EncodeInputModeStereo),
                    std::make_unique<juce::AudioParameterInt>(paramOutputMode, TRANS("Output Mode"), 0, Mach1EncodeOutputModeM1Spatial_60, Mach1EncodeOutputModeM1Spatial_8),
#endif
#ifdef ITD_PARAMETERS
                    std::make_unique<juce::AudioParameterBool>(paramITDActive, TRANS("ITD"), pannerSettings.itdActive),
                    std::make_unique<juce::AudioParameterFloat>(paramDelayTime,
                                                            TRANS("Delay Time (max)"),
                                                            juce::NormalisableRange<float>(0.0f, 10000.0f, 1.0f), pannerSettings.delayTime, "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "μS"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(1).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramDelayDistance,
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
    parameters.addParameterListener(paramX, this);
    parameters.addParameterListener(paramY, this);
    parameters.addParameterListener(paramGain, this);
    parameters.addParameterListener(paramAutoOrbit, this);
    parameters.addParameterListener(paramStereoOrbitAzimuth, this);
    parameters.addParameterListener(paramStereoSpread, this);
    parameters.addParameterListener(paramStereoInputBalance, this);
    parameters.addParameterListener(paramIsotropicEncodeMode, this);
    parameters.addParameterListener(paramEqualPowerEncodeMode, this);
#if defined(DYNAMIC_IO_PLUGIN_MODE) || defined(STREAMING_PANNER_PLUGIN)
    parameters.addParameterListener(paramInputMode, this);
    parameters.addParameterListener(paramOutputMode, this);
#endif
#ifdef ITD_PARAMETERS
    parameters.addParameterListener(paramITDActive, this);
    parameters.addParameterListener(paramDelayTime, this);
    parameters.addParameterListener(paramDelayDistance, this);
#endif

    // Setup defaults for Mach1Enecode API
    m1Encode.setPannerMode(pannerSettings.pannerMode);
    // assign pointer to Mach1Encode object
    pannerSettings.m1Encode = &m1Encode;
}

M1PannerAudioProcessor::~M1PannerAudioProcessor()
{
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
    int numInChans, numOutChans;
#ifdef CUSTOM_CHANNEL_LAYOUT
    numInChans = INPUT_CHANNELS;
    numOutChans = OUTPUT_CHANNELS;
#else
    numInChans = pannerSettings.m1Encode->getInputChannelsCount();
    numOutChans = pannerSettings.m1Encode->getOutputChannelsCount();
#endif
    
#ifdef STREAMING_PANNER_PLUGIN
    if (pannerSettings.inputType == Mach1EncodeInputModeMono){
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::mono());
    }
    else if (pannerSettings.inputType == Mach1EncodeInputModeStereo){
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
    }
    getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
#else
    //TODO: refactor this so it sets format per type instead of number of channels
    //TODO: refactor so there is a pool of selections for Quad/LCR, pool for ambisonics and a pool for surround
    if (numInChans == juce::AudioChannelSet::mono().size()){
        pannerSettings.m1Encode->setInputMode(Mach1EncodeInputModeMono);
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::mono());
        //removing this to solve mono/stereo plugin build issue on VST/AU/VST3
        //getBus(true, 1)->setCurrentLayout(juce::AudioChannelSet::mono());
    }
    else if (numInChans == juce::AudioChannelSet::stereo().size()){
        pannerSettings.m1Encode->setInputMode(Mach1EncodeInputModeStereo);
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::stereo());
    }
    else if (numInChans == juce::AudioChannelSet::createLCR().size()){
        pannerSettings.m1Encode->setInputMode(Mach1EncodeInputModeLCR);
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::createLCR());
    }
    else if (numInChans == juce::AudioChannelSet::quadraphonic().size()){
        // TODO: Add how we switch using getInputMode?
        pannerSettings.m1Encode->setInputMode(Mach1EncodeInputModeQuad);
//        if (mChanInputMode == QUAD) m1Encode.setInputMode(Mach1EncodeInputModeQuad);
//        if (mChanInputMode == LCRS) m1Encode.setInputMode(Mach1EncodeInputModeLCRS);
//        if (mChanInputMode == AFORMAT) m1Encode.setInputMode(Mach1EncodeInputModeAFormat);
//        if (mChanInputMode == BFORMAT_1OA_ACN) m1Encode.setInputMode(Mach1EncodeInputModeBFOAACN);
//        if (mChanInputMode == BFORMAT_1OA_FUMA) m1Encode.setInputMode(Mach1EncodeInputModeBFOAFUMA);
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::quadraphonic());
    }
    else if (numInChans == juce::AudioChannelSet::create5point0().size()){
        m1Encode.setInputMode(Mach1EncodeInputMode5dot0);
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::create5point0());
    }
    else if (numInChans == juce::AudioChannelSet::create5point1().size()){
        // TODO: Add how we switch using getInputMode?
        pannerSettings.m1Encode->setInputMode(Mach1EncodeInputMode5dot1Film);
//        if (mChanInputMode == FIVE_ONE_FILM) m1Encode.setInputMode(Mach1EncodeInputMode5dot1Film);
//        if (mChanInputMode == FIVE_ONE_SMPTE) m1Encode.setInputMode(Mach1EncodeInputMode5dot1SMTPE);
//        if (mChanInputMode == FIVE_ONE_DTS) m1Encode.setInputMode(Mach1EncodeInputMode5dot1DTS);
        //        if (inputMode == SIX_ZERO) m1Encode.setInputMode(); //TODO: add this
        //        if (inputMode == HEXAGON) m1Encode.setInputMode(); //TODO: add this
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::create5point1());
    }
    else if (numInChans == juce::AudioChannelSet::create7point1point2().size()){
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::create7point1point2());
    }
    else if (numInChans == juce::AudioChannelSet::ambisonic(2).size()){
        // TODO: Add how we switch using getInputMode?
        pannerSettings.m1Encode->setInputMode(Mach1EncodeInputModeB2OAACN);
//        if (mChanInputMode == BFORMAT_2OA_ACN) m1Encode.setInputMode(Mach1EncodeInputModeB2OAACN);
//        if (mChanInputMode == BFORMAT_2OA_FUMA) m1Encode.setInputMode(Mach1EncodeInputModeB2OAFUMA);
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::ambisonic(2));
    }
    else if (numInChans == juce::AudioChannelSet::ambisonic(3).size()){
        // TODO: Add how we switch using getInputMode?
        pannerSettings.m1Encode->setInputMode(Mach1EncodeInputModeB3OAACN);
//        if (mChanInputMode == BFORMAT_3OA_ACN) m1Encode.setInputMode(Mach1EncodeInputModeB3OAACN);
//        if (mChanInputMode == BFORMAT_3OA_FUMA) m1Encode.setInputMode(Mach1EncodeInputModeB3OAFUMA);
        getBus(true, 0)->setCurrentLayout(juce::AudioChannelSet::ambisonic(3));
    }
    pannerSettings.inputType = m1Encode.getInputMode();
    
    if(numOutChans == juce::AudioChannelSet::quadraphonic().size()){
        //TODO: add a multiplier to reduce the hot 4 channel signal
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Horizon_4);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::quadraphonic());
    } else if (numOutChans == juce::AudioChannelSet::create7point1().size()){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_8);
        if (hostType.isProTools()){
            getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::create7point1());
        } else {
            getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(8));
        }
    } else if(numOutChans == 12){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_12);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(12));
    } else if(numOutChans == 14){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_14);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(14));
    } else if(numOutChans == 18){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_18);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(18));
    } else if(numOutChans == 32){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_32);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(32));
    } else if(numOutChans == 36){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_36);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(36));
    } else if(numOutChans == 48){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_48);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(48));
    } else if(numOutChans == 60){
        pannerSettings.m1Encode->setOutputMode(Mach1EncodeOutputModeM1Spatial_60);
        getBus(false, 0)->setCurrentLayout(juce::AudioChannelSet::discreteChannels(60));
    }
    pannerSettings.outputType = m1Encode.getOutputMode();
#endif
    
    updateHostDisplay();
}

//==============================================================================
void M1PannerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
#if defined(DYNAMIC_IO_PLUGIN_MODE) || defined(STREAMING_PANNER_PLUGIN)
    createLayout();
#endif
    
    // can still be used to calculate coeffs even in STREAMING_PANNER_PLUGIN mode
    smoothedChannelCoeffs.resize(m1Encode.getInputChannelsCount());
    for (int input_channel = 0; input_channel < m1Encode.getInputChannelsCount(); input_channel++) {
        smoothedChannelCoeffs[input_channel].resize(m1Encode.getOutputChannelsCount());
        for (int output_channel = 0; output_channel < m1Encode.getOutputChannelsCount(); output_channel++) {
            smoothedChannelCoeffs[input_channel][output_channel].reset(sampleRate, (double)0.01);
        }
    }
    
#ifdef ITD_PARAMETERS
    mSampleRate = sampleRate;
    mDelayTimeSmoother.reset(samplesPerBlock);
    mDelayTimeSmoother.setValue(0);
    
    ring.reset(new RingBuffer(m1Encode.getOutputChannelsCount(), 64*sampleRate));
    ring->clear();

    // sample buffer for 2 seconds + MAX_NUM_CHANNELS buffers safety
    mDelayBuffer.setSize (m1Encode.getOutputChannelsCount(), (float)m1Encode.getOutputChannelsCount() * (samplesPerBlock + sampleRate), false, false);
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
        pannerSettings.azimuth = newValue;
    } else if (parameterID == paramElevation) {
        parameters.getParameter(paramElevation)->setValue(newValue);
        pannerSettings.elevation = newValue;
    } else if (parameterID == paramDiverge) {
        parameters.getParameter(paramDiverge)->setValue(newValue);
        pannerSettings.diverge = newValue;
    } else if (parameterID == paramGain) {
        parameters.getParameter(paramGain)->setValue(newValue);
        pannerSettings.gain = newValue;
    } else if (parameterID == paramX) {
        parameters.getParameter(paramX)->setValue(newValue);
        pannerSettings.x = newValue;
        //TODO: XYtoRD
    } else if (parameterID == paramY) {
        parameters.getParameter(paramY)->setValue(newValue);
        pannerSettings.y = newValue;
        //TODO: XYtoRD
    } else if (parameterID == paramAutoOrbit) {
        parameters.getParameter(paramAutoOrbit)->setValue(newValue);
        pannerSettings.autoOrbit = newValue;
    } else if (parameterID == paramStereoOrbitAzimuth) {
        parameters.getParameter(paramStereoOrbitAzimuth)->setValue(newValue);
        pannerSettings.stereoOrbitAzimuth = newValue;
    } else if (parameterID == paramStereoSpread) {
        parameters.getParameter(paramStereoSpread)->setValue(newValue);
        pannerSettings.stereoSpread = newValue;
    } else if (parameterID == paramStereoInputBalance) {
        parameters.getParameter(paramStereoInputBalance)->setValue(newValue);
        //TODO: add this via processing?
    } else if (parameterID == paramIsotropicEncodeMode) {
        parameters.getParameter(paramIsotropicEncodeMode)->setValue(newValue);
        pannerSettings.isotropicMode = newValue;
        // set in UI
    } else if (parameterID == paramEqualPowerEncodeMode) {
        parameters.getParameter(paramEqualPowerEncodeMode)->setValue(newValue);
        // set in UI
#if defined(DYNAMIC_IO_PLUGIN_MODE) || defined(STREAMING_PANNER_PLUGIN)
    } else if (parameterID == paramInputMode) {
        int inputChannelCount = parameters.getParameter(paramInputMode)->getValue();
        Mach1EncodeInputModeType input;
        if (inputChannelCount == 1) {
            input = Mach1EncodeInputModeMono;
        } else if (inputChannelCount == 2) {
            input = Mach1EncodeInputModeStereo;
        } else if (inputChannelCount == 3) {
            input = Mach1EncodeInputModeLCR;
        } else if (inputChannelCount == 4) {
            input = Mach1EncodeInputModeQuad;
        } else if (inputChannelCount == 5) {
            input = Mach1EncodeInputMode5dot0;
        } else if (inputChannelCount == 6) {
            input = Mach1EncodeInputMode5dot1Film;
        }
        m1Encode.setInputMode(input);
        pannerSettings.inputType = input;
        createLayout();
    } else if (parameterID == paramOutputMode) {
        int outputChannelCount = parameters.getParameter(paramOutputMode)->getValue();
        Mach1EncodeOutputModeType output;
        if (outputChannelCount == 1) {
            output = Mach1EncodeOutputModeM1Spatial_8;
        } else if (outputChannelCount == 2) {
            output = Mach1EncodeOutputModeM1Horizon_4;
        } else if (outputChannelCount == 3) {
            output = Mach1EncodeOutputModeM1Spatial_12;
        } else if (outputChannelCount == 4) {
            output = Mach1EncodeOutputModeM1Spatial_14;
        } else if (outputChannelCount == 5) {
            output = Mach1EncodeOutputModeM1Spatial_18;
        } else if (outputChannelCount == 6) {
            output = Mach1EncodeOutputModeM1Spatial_32;
        } else if (outputChannelCount == 7) {
            output = Mach1EncodeOutputModeM1Spatial_36;
        } else if (outputChannelCount == 8) {
            output = Mach1EncodeOutputModeM1Spatial_48;
        } else if (outputChannelCount == 9) {
            output = Mach1EncodeOutputModeM1Spatial_60;
        }
        m1Encode.setOutputMode(output);
        pannerSettings.outputType = output;
        createLayout();
#endif
    }
    pannerSettings.m1Encode = &m1Encode;
}

#ifndef CUSTOM_CHANNEL_LAYOUT
bool M1PannerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    Mach1Encode configTester;
    
    // block plugin if input or output is disabled on construction
    if (layouts.getMainInputChannelSet()  == juce::AudioChannelSet::disabled()
     || layouts.getMainOutputChannelSet() == juce::AudioChannelSet::disabled())
        return false;
    
    // manually maintained for-loop of first enum element to last enum element
    // TODO: brainstorm a way to not require manual maintaining of listed enum elements
    for (int inputEnum = Mach1EncodeInputModeMono; inputEnum != Mach1EncodeInputMode5dot1SMTPE; inputEnum++ ) {
        configTester.setInputMode(static_cast<Mach1EncodeInputModeType>(inputEnum));
        // test each input, if the input has the number of channels as the input testing layout has move on to output testing
        if (layouts.getMainInputChannels() == configTester.getInputChannelsCount()) {
            for (int outputEnum = Mach1EncodeOutputModeM1Horizon_4; outputEnum != Mach1EncodeOutputModeM1Spatial_32; outputEnum++ ) {
                // test each output
               configTester.setOutputMode(static_cast<Mach1EncodeOutputModeType>(outputEnum));
                if (layouts.getMainOutputChannels() == configTester.getOutputChannelsCount()){
                    return true;
                }
            }
        }
    }
    return false;
}
#endif

void M1PannerAudioProcessor::fillChannelOrderArray(int numOutputChannels) {
    orderOfChans.resize(numOutputChannels);
    chanIndexs.resize(numOutputChannels);
    if(numOutputChannels == 8) {
        //Pro Tools
        if (hostType.isProTools()){
            orderOfChans[0] = juce::AudioChannelSet::ChannelType::left;
            orderOfChans[1] = juce::AudioChannelSet::ChannelType::centre;
            orderOfChans[2] = juce::AudioChannelSet::ChannelType::right;
            orderOfChans[3] = juce::AudioChannelSet::ChannelType::leftSurroundSide;
            orderOfChans[4] = juce::AudioChannelSet::ChannelType::rightSurroundSide;
            orderOfChans[5] = juce::AudioChannelSet::ChannelType::leftSurroundRear;
            orderOfChans[6] = juce::AudioChannelSet::ChannelType::rightSurroundRear;
            orderOfChans[7] = juce::AudioChannelSet::ChannelType::LFE;
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
        juce::AudioChannelSet chanset = getBus(false, 0)->getCurrentLayout();
        for (int i = 0; i < numOutputChannels; i ++) {
            chanIndexs[i] = chanset.getChannelIndexForType(orderOfChans[i]);
        }
    } else if (numOutputChannels == 4){
        // Layout for Pro Tools
        if (hostType.isProTools()) {
            orderOfChans[0] = juce::AudioChannelSet::ChannelType::left;
            orderOfChans[1] = juce::AudioChannelSet::ChannelType::right;
            orderOfChans[2] = juce::AudioChannelSet::ChannelType::rightSurround;
            orderOfChans[3] = juce::AudioChannelSet::ChannelType::leftSurround;
        } else {
            //Layout for Reaper et al
            orderOfChans[0] = juce::AudioChannelSet::ChannelType::left;
            orderOfChans[1] = juce::AudioChannelSet::ChannelType::right;
            orderOfChans[2] = juce::AudioChannelSet::ChannelType::leftSurround;
            orderOfChans[3] = juce::AudioChannelSet::ChannelType::rightSurround;
        }

        juce::AudioChannelSet chanset = getBus(false, 0)->getCurrentLayout();
        for (int i = 0; i < numOutputChannels; i ++) {
            chanIndexs[i] = chanset.getChannelIndexForType(orderOfChans[i]);
        }
    } else {
        for (int i = 0; i < numOutputChannels; ++i){
            orderOfChans[i] = juce::AudioChannelSet::ChannelType::discreteChannel0;
            chanIndexs[i] = i;
        }
    }
}

void M1PannerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Fix host specific channel ordering issues
    fillChannelOrderArray(totalNumOutputChannels);

    // if you've got more output channels than input clears extra outputs
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());
    
    // Set temp values for processing
    float _diverge = pannerSettings.diverge;
    float _gain = juce::Decibels::decibelsToGain(pannerSettings.gain);

    // TODO: Check for a monitor if none is connected
    
    if (monitorSettings.monitor_mode == 2) { // StereoSafe mode is on
        //store diverge for gain
        float abs_diverge = fabsf((_diverge - -100.0f) / (100.0f - -100.0f)); // ?
        //Setup for stereoSafe diverge range to gain
        _gain = _gain - (abs_diverge * 6.0);
        //Set Diverge to 0 after using Diverge for Gain
        _diverge = 0;
    }
    
    // parameters that can be automated will get their values updated from PannerSettings->Parameter
    m1Encode.setAzimuthDegrees(pannerSettings.azimuth);
    m1Encode.setElevationDegrees(pannerSettings.elevation);
    m1Encode.setDiverge(_diverge); // using _diverge in case monitorMode was used

    m1Encode.setAutoOrbit(pannerSettings.autoOrbit);
    m1Encode.setOutputGain(_gain, true);
    m1Encode.setOrbitRotationDegrees(pannerSettings.stereoOrbitAzimuth);
    m1Encode.setStereoSpread(pannerSettings.stereoSpread);
    // TODO: logic for usage of `paramStereoInputBalance`
    
    m1Encode.generatePointResults();
    auto gainCoeffs = m1Encode.getGains();

    // vector of input channel buffers
    std::vector<const float*> buffers;
    juce::AudioSampleBuffer mainInput = getBusBuffer(buffer, true, 0);
    juce::AudioChannelSet inputLayout = getChannelLayoutOfBus(true, 0);
    audioDataIn.resize(m1Encode.getInputChannelsCount());
    
    juce::AudioSampleBuffer mainOutput = getBusBuffer(buffer, false, 0);
    float ** outBuffer = mainOutput.getArrayOfWritePointers();
    // vector of output channel buffers
//    std::vector<float*> outBuffer; // reset vector
//    for (int output_channel = 0; output_channel < totalNumOutputChannels; output_channel++) {
//        juce::AudioSampleBuffer mainOutput = getBusBuffer(buffer, false, output_channel);
//        outBuffer.push_back(mainOutput.getWritePointer(output_channel));
//
//        // clear all old output samples
//        mainOutput.clear();
//    }

#ifdef ITD_PARAMETERS
    mDelayTimeSmoother.setTargetValue(delayTimeParameter->get());
    const float udtime = mDelayTimeSmoother.getNextValue() * mSampleRate/1000000; // number of samples in a microsecond * number of microseconds
#endif
    
#ifdef STREAMING_PANNER_PLUGIN
    // TODO: safely block the input from passing to output
#else
    /// INTERNAL_SPATIAL_PROCESSING
    
    // input channel setup loop
    for (int input_channel = 0; input_channel < m1Encode.getInputChannelsCount(); input_channel++){
        // Copy input data to additional buffer
        audioDataIn[input_channel].resize(mainInput.getNumSamples());
        memcpy(audioDataIn[input_channel].data(), mainInput.getReadPointer(input_channel), sizeof(float) * mainInput.getNumSamples());
        // TODO: figure out how to best use getChannelIndexForType() instead of literal index?
        // Get the current input channel index audio data buffer
        const float* newChannelInputBuffer = audioDataIn[input_channel].data();
        buffers.push_back(newChannelInputBuffer);
        
        // output channel setup loop
        for (int output_channel = 0; output_channel < m1Encode.getOutputChannelsCount(); output_channel++){
            // TODO: add channel reorder here?
            smoothedChannelCoeffs[input_channel][output_channel].setTargetValue(gainCoeffs[input_channel][output_channel] * _gain);
        }
    }
    
    // Process Buffer loop section
    if(pannerSettings.m1Encode->getInputMode() == Mach1EncodeInputModeMono){
        buffers[0] = audioDataIn[0].data();
        // Default alway add side chain bus.
    } else if(pannerSettings.m1Encode->getInputMode() == Mach1EncodeInputModeStereo && mainInput.getNumChannels() >= 2){
        buffers[0] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::left)].data();
        buffers[1] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::right)].data();
    } else if (pannerSettings.m1Encode->getInputMode() == Mach1EncodeInputModeLCR && mainInput.getNumChannels() >= 3){
        buffers[0] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::left)].data();
        buffers[1] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::centre)].data();
        buffers[2] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::right)].data();
    } else if (pannerSettings.m1Encode->getInputChannelsCount() == 4 && mainInput.getNumChannels() >= 4){
        buffers[0] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::left)].data();
        buffers[1] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::right)].data();
        buffers[2] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::leftSurround)].data();
        buffers[3] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::rightSurround)].data();
    } else if (pannerSettings.m1Encode->getInputChannelsCount() == 5 && mainInput.getNumChannels() >= 5){
        buffers[0] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::left)].data();
        buffers[1] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::centre)].data();
        buffers[2] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::right)].data();
        buffers[3] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::leftSurround)].data();
        buffers[4] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::rightSurround)].data();
    } else if (pannerSettings.m1Encode->getInputChannelsCount() == 6 && mainInput.getNumChannels() >= 6){ // we use this instead of == to ensure HOSTS can support the plugin even when input channel count is > than needed
        buffers[0] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::left)].data();
        buffers[1] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::centre)].data();
        buffers[2] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::right)].data();
        buffers[3] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::leftSurround)].data();
        buffers[4] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::rightSurround)].data();
        buffers[5] = audioDataIn[inputLayout.getChannelIndexForType(juce::AudioChannelSet::LFE)].data();
    }

    // processing loop
    for (int input_channel = 0; input_channel < m1Encode.getInputChannelsCount(); input_channel++){
        for (int sample = 0; sample < buffer.getNumSamples(); sample++){
            float inValue = buffers[input_channel][sample];
            for (int output_channel = 0; output_channel < m1Encode.getOutputChannelsCount(); output_channel++){
                // TODO: add channel reorder here?
                float inGain = smoothedChannelCoeffs[input_channel][output_channel].getNextValue();

                outBuffer[output_channel][sample] = inValue * inGain;
            }
        }
    }
    
#ifdef ITD_PARAMETERS
    //SIMPLE DELAY
    // scale delayCoeffs to be normalized
    for (int i = 0; i < m_i_numInChans; i++) {
        for (int o = 0; o < m_i_numOutChans; o++) {
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
            for (auto channel = 0; channel < m_i_numOutChans; channel++) {
                ring->pushSample(channel, outBuffer[channel][sample]);
            }
            for (int channel = 0; channel < m_i_numOutChans; channel++) {
                outBuffer[channel][sample] = (outBuffer[channel][sample] * 0.707106781) + (ring->getSampleAtDelay(channel, udtime * delayCoeffs[0][channel]) * 0.707106781); // pan-law applied via `0.707106781`
            }
            ring->increment();
        }
    }
#endif // end of ITD_PARAMETERS
    
#endif // end of processing flow
    
    // update meters
    juce::AudioBuffer<float> buf(buffer.getArrayOfWritePointers(), buffer.getNumChannels(), buffer.getNumSamples());
    outputMeterValuedB.resize(buffer.getNumChannels());
    for (int j = 0; j < buffer.getNumChannels(); j++) {
        outputMeterValuedB.set(j, j < buffer.getNumChannels() ? juce::Decibels::gainToDecibels(buf.getRMSLevel(j, 0, buffer.getNumSamples())) : -144 );
    }
    
    // update m1encode for UI
    pannerSettings.m1Encode = &m1Encode;
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
    return new M1PannerAudioProcessorEditor (*this, pannerSettings.pannerMode, pannerSettings.inputType);
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

void M1PannerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    // You should use this method to store your parameters in the memory block.
    juce::MemoryOutputStream stream(destData, false);
    // DO NOT CHANGE THIS NUMBER, it is not a version tracker but a version threshold for supporting
    // backwards compatible automation data in PT
    stream.writeString("1.5.1"); // write current prefix

    juce::XmlElement root("Root");

    addXmlElement(root, paramAzimuth, juce::String(pannerSettings.azimuth));
    addXmlElement(root, paramElevation, juce::String(pannerSettings.elevation));
    addXmlElement(root, paramDiverge, juce::String(pannerSettings.diverge));
    addXmlElement(root, paramGain, juce::String(pannerSettings.gain));
    addXmlElement(root, paramX, juce::String(pannerSettings.x));
    addXmlElement(root, paramY, juce::String(pannerSettings.y));
    addXmlElement(root, paramStereoOrbitAzimuth, juce::String(pannerSettings.stereoOrbitAzimuth));
    addXmlElement(root, paramStereoSpread, juce::String(pannerSettings.stereoSpread));
    addXmlElement(root, paramStereoInputBalance, juce::String(pannerSettings.stereoInputBalance));
    addXmlElement(root, paramAutoOrbit, juce::String(pannerSettings.autoOrbit ? 1 : 0));
    addXmlElement(root, paramIsotropicEncodeMode, juce::String(pannerSettings.isotropicMode ? 1 : 0));
    addXmlElement(root, paramEqualPowerEncodeMode, juce::String(pannerSettings.equalpowerMode ? 1 : 0));

#if defined(DYNAMIC_IO_PLUGIN_MODE) || defined(STREAMING_PANNER_PLUGIN)
    addXmlElement(root, paramInputMode, juce::String(pannerSettings.inputType));
    addXmlElement(root, paramOutputMode, juce::String(pannerSettings.outputType));
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

    if (!prefix.isEmpty() && prefix == "1.5.1") {
        // new method
        juce::XmlDocument doc(input.readString());
        std::unique_ptr<juce::XmlElement> root(doc.getDocumentElement());
    
        pannerSettings.azimuth = getParameterDoubleFromXmlElement(root.get(), paramAzimuth, pannerSettings.azimuth);
        pannerSettings.elevation = getParameterDoubleFromXmlElement(root.get(), paramElevation, pannerSettings.elevation);
        pannerSettings.diverge = getParameterDoubleFromXmlElement(root.get(), paramDiverge, pannerSettings.diverge);
        pannerSettings.gain = getParameterDoubleFromXmlElement(root.get(), paramGain, pannerSettings.gain);
        pannerSettings.x = getParameterDoubleFromXmlElement(root.get(), paramX, pannerSettings.x);
        pannerSettings.y = getParameterDoubleFromXmlElement(root.get(), paramY, pannerSettings.y);
        pannerSettings.stereoOrbitAzimuth = getParameterDoubleFromXmlElement(root.get(), paramStereoOrbitAzimuth, pannerSettings.stereoOrbitAzimuth);
        pannerSettings.stereoSpread = getParameterDoubleFromXmlElement(root.get(), paramStereoSpread, pannerSettings.stereoSpread);
        pannerSettings.stereoInputBalance = getParameterDoubleFromXmlElement(root.get(), paramStereoInputBalance, pannerSettings.stereoInputBalance);
        pannerSettings.autoOrbit = getParameterIntFromXmlElement(root.get(), paramAutoOrbit, pannerSettings.autoOrbit);
        pannerSettings.isotropicMode = getParameterIntFromXmlElement(root.get(), paramIsotropicEncodeMode, pannerSettings.isotropicMode);
        pannerSettings.equalpowerMode = getParameterIntFromXmlElement(root.get(), paramEqualPowerEncodeMode, pannerSettings.equalpowerMode);

#if defined(DYNAMIC_IO_PLUGIN_MODE) || defined(STREAMING_PANNER_PLUGIN)
        //pannerSettings.inputType = getParameterDoubleFromXmlElement(root.get(), paramInputMode, pannerSettings.inputType);
        //pannerSettings.outputType = getParameterDoubleFromXmlElement(root.get(), paramOutputMode, pannerSettings.outputType);
#endif
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new M1PannerAudioProcessor();
}
