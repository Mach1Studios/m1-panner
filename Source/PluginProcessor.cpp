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
juce::String M1PannerAudioProcessor::paramInputMode("inputMode");
juce::String M1PannerAudioProcessor::paramOutputMode("outputMode");

//==============================================================================
M1PannerAudioProcessor::M1PannerAudioProcessor()
     : AudioProcessor (BusesProperties()
                        .withInput("Default Input", juce::AudioChannelSet::stereo(), true)
                        #if (JucePlugin_Build_AAX || JucePlugin_Build_RTAS)
                        .withOutput("Default Output", juce::AudioChannelSet::create7point1(), true)
                        #else
                        .withOutput("Mach1 Out", AudioChannelSet::discreteChannels(8), true)
                        #endif
//                       if (juce::PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_AAX || juce::PluginHostType::getPluginLoadedAs() == AudioProcessor::wrapperType_RTAS) {
//                            .withOutput("Default Output", juce::AudioChannelSet::create7point1(), true)
//                        // manually declare confirmed multichannel DAWs
//                       } else if (hostType == JUCEPluginHost || hostType == Reaper || hostType == SteinbergNuendoGeneric || hostType == Ardour || hostType == DaVinciResolve) {
//                            .withOutput("Mach1 Out", AudioChannelSet::discreteChannels(8), true)
//                        // fallback for all other DAWs to output stereo
//                       } else {
//                            .withOutput("Output", juce::AudioChannelSet::stereo(), true)
//                       }
                       ),
    parameters(*this, &mUndoManager, juce::Identifier("M1Panner"),
               {
                    std::make_unique<juce::AudioParameterFloat>(paramAzimuth,
                                                            TRANS("Azimuth"),
                                                            juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), mAzimuth.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramElevation,
                                                            TRANS("Elevation"),
                                                            juce::NormalisableRange<float>(-90.0f, 90.0f, 0.01f), mElevation.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramDiverge,
                                                            TRANS("Diverge"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), mDiverge.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramX,
                                                            TRANS("X"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), mX.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramY,
                                                            TRANS("Y"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), mY.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramGain,
                                                            TRANS ("Input Gain"),
                                                    juce::NormalisableRange<float>(-100.0f, 6.0f, 0.1f, std::log (0.5f) / std::log (100.0f / 106.0f)),
                                                            mGain.get(), "",
                                                    juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + " dB"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters (3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterBool>(paramAutoOrbit, TRANS("Auto Orbit"), mAutoOrbit.get()),
                    std::make_unique<juce::AudioParameterFloat>(paramStereoOrbitAzimuth,
                                                            TRANS("Stereo Orbit Rotation"),
                                                            juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), mStereoOrbitAzimuth.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramStereoSpread,
                                                            TRANS("Stereo Spread"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), mStereoSpread.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramStereoInputBalance,
                                                            TRANS("Stereo Input Balance"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), mStereoInputBalance.get(), "", juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterBool>(paramIsotropicEncodeMode, TRANS("Isotropic Encode Mode"), mIsotropicEncodeMode.get()),
                    std::make_unique<juce::AudioParameterBool>(paramEqualPowerEncodeMode, TRANS("Equal Power Encode Mode"), mEqualPowerEncodeMode.get()),
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
    
    // Setup for Mach1Enecode API
    m1Encode.setInputMode(pannerSettings.inputType);
    m1Encode.setOutputMode(pannerSettings.outputType);
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
void M1PannerAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Use this method as the place to do any pre-playback
    smoothedChannelCoeffs.resize(m1Encode.getOutputChannelsCount());
    
    for (int i = 0; i < m1Encode.getOutputChannelsCount(); i++) {
        smoothedChannelCoeffs[i].reset(sampleRate, (double)0.01);
    }
}

void M1PannerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void M1PannerAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    if (parameterID == paramAzimuth) {
        pannerSettings.azimuth = newValue;
        mAzimuth = pannerSettings.azimuth;
        parameters.getParameter(paramAzimuth)->setValue(mAzimuth.get());
        m1Encode.setAzimuthDegrees(mAzimuth.get());
    } else if (parameterID == paramElevation) {
        pannerSettings.elevation = newValue;
        mElevation = pannerSettings.elevation;
        parameters.getParameter(paramElevation)->setValue(mElevation.get());
        m1Encode.setElevationDegrees(mElevation.get());
    } else if (parameterID == paramDiverge) {
        pannerSettings.diverge = newValue;
        mDiverge = pannerSettings.diverge;
        parameters.getParameter(paramDiverge)->setValue(mDiverge.get());
        m1Encode.setDiverge(mDiverge.get());
    } else if (parameterID == paramGain) {
        pannerSettings.gain = newValue;
        mGain = pannerSettings.gain;
        parameters.getParameter(paramGain)->setValue(mGain.get());
        m1Encode.setOutputGain(mGain.get(), true);
    } else if (parameterID == paramX) {
        pannerSettings.x = newValue;
        mX = pannerSettings.x;
        parameters.getParameter(paramX)->setValue(mX.get());
        //TODO: XYtoRD
    } else if (parameterID == paramY) {
        pannerSettings.y = newValue;
        mY = pannerSettings.y;
        parameters.getParameter(paramY)->setValue(mY.get());
        //TODO: XYtoRD
    } else if (parameterID == paramAutoOrbit) {
        pannerSettings.autoOrbit = newValue;
        mAutoOrbit = pannerSettings.autoOrbit;
        parameters.getParameter(paramAutoOrbit)->setValue(mAutoOrbit.get());
        m1Encode.setAutoOrbit(mAutoOrbit.get());
    } else if (parameterID == paramStereoOrbitAzimuth) {
        pannerSettings.stereoOrbitAzimuth = newValue;
        mStereoOrbitAzimuth = pannerSettings.stereoOrbitAzimuth;
        parameters.getParameter(paramStereoOrbitAzimuth)->setValue(mStereoOrbitAzimuth.get());
        m1Encode.setOrbitRotationDegrees(mStereoOrbitAzimuth.get());
    } else if (parameterID == paramStereoSpread) {
        pannerSettings.stereoSpread = newValue;
        mStereoSpread = pannerSettings.stereoSpread;
        parameters.getParameter(paramStereoSpread)->setValue(mStereoSpread.get());
        m1Encode.setStereoSpread(mStereoSpread.get());
    } else if (parameterID == paramStereoInputBalance) {
        pannerSettings.stereoInputBalance = newValue;
        mStereoInputBalance = pannerSettings.stereoInputBalance;
        parameters.getParameter(paramStereoInputBalance)->setValue(mStereoInputBalance.get());
        //TODO: add this via processing?
    } else if (parameterID == paramIsotropicEncodeMode) {
        pannerSettings.isotropicMode = newValue;
        mIsotropicEncodeMode = pannerSettings.isotropicMode;
        parameters.getParameter(paramIsotropicEncodeMode)->setValue(mIsotropicEncodeMode.get());
        // set in UI
    } else if (parameterID == paramEqualPowerEncodeMode) {
        pannerSettings.equalpowerMode = newValue;
        mEqualPowerEncodeMode = pannerSettings.equalpowerMode;
        parameters.getParameter(paramEqualPowerEncodeMode)->setValue(mEqualPowerEncodeMode.get());
        // set in UI
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
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

void M1PannerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();
    
    // Set temp values for processing
    float _azimuth = parameters.getParameter(paramAzimuth)->getValue();
    float _elevation = parameters.getParameter(paramElevation)->getValue();
    float _diverge = parameters.getParameter(paramDiverge)->getValue();
    float _gain = juce::Decibels::decibelsToGain(parameters.getParameter(paramGain)->getValue());
    
    if (monitorSettings.monitor_mode == 2) { // StereoSafe mode is on
        //store diverge for gain
        float abs_diverge = fabsf((parameters.getParameter(paramDiverge)->getValue() - -100.0f) / (100.0f - -100.0f));
        //Setup for stereoSafe diverge range to gain
        _gain = _gain - (abs_diverge * 6.0);
        //Set Diverge to 0 after using Diverge for Gain
        _diverge = 0;
    }

    // Committing Mach1Encode settings for processing
    m1Encode.setInputMode(pannerSettings.inputType);
    m1Encode.setOutputMode(pannerSettings.outputType);
    m1Encode.setPannerMode(pannerSettings.pannerMode);
    
    // parameters that can be automated will get their values updated from PannerSettings->Parameter
    m1Encode.setAzimuthDegrees(parameters.getParameter(paramAzimuth)->getValue());
    m1Encode.setElevationDegrees(parameters.getParameter(paramElevation)->getValue());
    m1Encode.setDiverge(_diverge); // using _diverge in case monitorMode was used

    m1Encode.setAutoOrbit(parameters.getParameter(paramAutoOrbit)->getValue());
    m1Encode.setOutputGain(parameters.getParameter(paramGain)->getValue(), true);
    m1Encode.setOrbitRotationDegrees(parameters.getParameter(paramStereoOrbitAzimuth)->getValue());
    m1Encode.setStereoSpread(parameters.getParameter(paramStereoSpread)->getValue());
    // TODO: logic for usage of `paramStereoInputBalance`
    
    m1Encode.generatePointResults();
    auto gainCoeffs = m1Encode.getGains();

    // vector of input channel buffers
    std::vector<const float*> buffers;
    juce::AudioSampleBuffer mainInput = getBusBuffer(buffer, true, 0);
    juce::AudioChannelSet inputLayout = getChannelLayoutOfBus(true, 0);
    audioDataIn.resize(m1Encode.getInputChannelsCount());
    smoothedChannelCoeffs.resize(m1Encode.getInputChannelsCount());
    // vector of output channel buffers
    juce::AudioSampleBuffer mainOutput = getBusBuffer(buffer, false, 0);
    float ** outBuffer = mainOutput.getArrayOfWritePointers();
    
    // clear all old output samples
    mainOutput.clear();
    
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
            smoothedChannelCoeffs[output_channel].setTargetValue(gainCoeffs[input_channel][output_channel] * _gain);
        }
    }
    
    // processing loop
    for (int input_channel = 0; input_channel < m1Encode.getInputChannelsCount(); input_channel++){
        for (int sample = 0; sample < mainOutput.getNumSamples(); sample++){
            float inValue = buffers[input_channel][sample];
            for (int output_channel = 0; output_channel < m1Encode.getOutputChannelsCount(); output_channel++){
                // TODO: add channel reorder here?
                float inGain = smoothedChannelCoeffs[input_channel].getNextValue();
                outBuffer[output_channel][sample] = inValue * inGain;
            }
        }
    }
    
    // update meters
    juce::AudioBuffer<float> buf(buffer.getArrayOfWritePointers(), mainOutput.getNumChannels(), mainOutput.getNumSamples());
    outputMeterValuedB.resize(mainOutput.getNumChannels());
    for (int j = 0; j < mainOutput.getNumChannels(); j++) {
        outputMeterValuedB.set(j, j < mainOutput.getNumChannels() ? juce::Decibels::gainToDecibels(buf.getRMSLevel(j, 0, mainOutput.getNumSamples())) : -144 );
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
    return new M1PannerAudioProcessorEditor (*this, pannerSettings.pannerMode, pannerSettings.inputType);
}

//==============================================================================
void M1PannerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
    juce::MemoryOutputStream stream(destData, false);
    parameters.state.writeToStream(stream);
}

void M1PannerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
    juce::ValueTree tree = juce::ValueTree::readFromData(data, sizeInBytes);
    if (tree.isValid()) {
        parameters.state = tree;
        //TODO: restore settings back to pannerSettings?
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new M1PannerAudioProcessor();
}
