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
juce::String M1PannerAudioProcessor::paramPannerMode("pannerMode");
juce::String M1PannerAudioProcessor::paramInputMode("inputMode");
juce::String M1PannerAudioProcessor::paramOutputMode("outputMode");

//==============================================================================
M1PannerAudioProcessor::M1PannerAudioProcessor()
     : AudioProcessor (BusesProperties()
                       .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                       if (hostType == AvidProTools) {
                            .withOutput("Default Output", juce::AudioChannelSet::create7point1(), true)
                        // manually declare confirmed multichannel DAWs
                       } else if (hostType == JUCEPluginHost || hostType == Reaper || hostType == SteinbergNuendoGeneric || hostType == Ardour || hostType == DaVinciResolve) {
                            .withOutput("Mach1 Out", AudioChannelSet::discreteChannels(8), true)
                        // fallback for all other DAWs to output stereo
                       } else {
                            .withOutput("Output", juce::AudioChannelSet::stereo(), true)
                       }
                       ),
    parameters(*this, &mUndoManager, juce::Identifier("M1Panner"),
               {
                    std::make_unique<juce::AudioParameterFloat>(paramAzimuth,
                                                            TRANS("Azimuth"),
                                                            juce::NormalisableRange<float>(-180.0f, 180.0f, 0.01f), mAzimuth.get(), "",                                       juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramElevation,
                                                            TRANS("Elevation"),
                                                            juce::NormalisableRange<float>(-90.0f, 90.0f, 0.01f), mElevation.get(), "",                                       juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + "°"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramDiverge,
                                                            TRANS("Diverge"),
                                                            juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01f), mDiverge.get(), "",                                       juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1); },
                                                            [](const juce::String& t) { return t.dropLastCharacters(3).getFloatValue(); }),
                    std::make_unique<juce::AudioParameterFloat>(paramGain,
                                                            TRANS ("Input Gain"),
                                                    juce::NormalisableRange<float>(-100.0f, 6.0f, 0.1f, std::log (0.5f) / std::log (100.0f / 106.0f)),
                                                            mGain.get(), "",
                                                    juce::AudioProcessorParameter::genericParameter,
                                                            [](float v, int) { return juce::String (v, 1) + " dB"; },
                                                            [](const juce::String& t) { return t.dropLastCharacters (3).getFloatValue(); }),
               })
{
    parameters.addParameterListener(paramAzimuth, this);
    parameters.addParameterListener(paramElevation, this);
    parameters.addParameterListener(paramDiverge, this);
    parameters.addParameterListener(paramGain, this);

    // Setup for Mach1Enecode API
    m1Encode.setInputMode(pannerSettings.inputType);
    m1Encode.setOutputMode(pannerSettings.outputType);
    m1Encode.setPannerMode(pannerSettings.pannerMode);
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
    // initialisation that you need..
}

void M1PannerAudioProcessor::releaseResources()
{
    // When playback stops, you can use this as an opportunity to free up any
    // spare memory, etc.
}

void M1PannerAudioProcessor::parameterChanged(const juce::String &parameterID, float newValue)
{
    if (parameterID == paramAzimuth) {
        mAzimuth = newValue;
    }
    else if (parameterID == paramElevation) {
        mElevation = newValue;
    }
    else if (parameterID == paramDiverge) {
        mDiverge = newValue;
    }
    else if (parameterID == paramGain) {
        mGain = newValue;
    }
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool M1PannerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    //TODO: setup logic to return true for all Mach1Encode I/O combos and return false for undefined I/O combos
}
#endif

void M1PannerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // In case we have more outputs than inputs, this code clears any output
    // channels that didn't contain input data, (because these aren't
    // guaranteed to be empty - they may contain garbage).
    // This is here to avoid people getting screaming feedback
    // when they first compile a plugin, but obviously you don't need to keep
    // this code if your algorithm always overwrites all the output channels.
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // This is the place where you'd normally do the guts of your plugin's
    // audio processing...
    // Make sure to reset the state if your inner loop is processing
    // the samples and the outer loop is handling the channels.
    // Alternatively, you can process the samples with the channels
    // interleaved by keeping the same state.
    for (int channel = 0; channel < totalNumInputChannels; ++channel)
    {
        auto* channelData = buffer.getWritePointer (channel);

        // ..do something to the data...
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
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new M1PannerAudioProcessor();
}
