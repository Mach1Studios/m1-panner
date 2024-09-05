/*
  ==============================================================================

    LegacyParameters.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

// This header contains the refactored logic for `setStateInformation` and `getStateInformation` to help make the PluginProcessor more readable

class AudioParameterCustom
{
public:
    std::type_index type_index = std::type_index(typeid(NULL));

    virtual void update() {}
    virtual void update(void* ptr) {}
    virtual ~AudioParameterCustom() {}
};

class AudioParameterCustomFloat : public AudioParameterCustom, public juce::AudioParameterFloat
{
    float* ptr = nullptr;
    float data = 0;

public:
    AudioParameterCustomFloat(float* ptr_, const juce::String& parameterID, const juce::String& name, juce::NormalisableRange<float> normalisableRange, float defaultValue, const juce::String& label = juce::String(), Category category = juce::AudioProcessorParameter::genericParameter, std::function<juce::String(float value, int maximumStringLength)> stringFromValue = nullptr, std::function<float(const juce::String& text)> valueFromString = nullptr) : juce::AudioParameterFloat(parameterID, name, normalisableRange, defaultValue, label, category, stringFromValue, valueFromString)
    {
        data = defaultValue;
        ptr = ptr_;
        type_index = std::type_index(typeid(float));
    };
    bool shouldAutomate = false;
    operator float() const noexcept { return juce::AudioParameterFloat::get(); }
    float getNormalized() { return getNormalisableRange().convertTo0to1(juce::AudioParameterFloat::get()); }

    AudioParameterCustomFloat& operator=(float newValue)
    {
        float val = convertFrom0to1(convertTo0to1(newValue));
        (juce::AudioParameterFloat::operator=)(val);

        if (ptr)
        {
            data = val;
            *ptr = val;
        }
        return *this;
    }

    void update() override
    {
        if (ptr && *ptr != data)
        {
            this->operator=(*ptr);
        }
    }

    void update(void* ptr_) override
    {
        if (ptr == ptr_)
        {
            this->operator=(*ptr);
        }
    }

    bool isAutomatable() const override
    {
        return shouldAutomate;
    }
};

class AudioParameterCustomBool : public AudioParameterCustom, public juce::AudioParameterBool
{
    bool* ptr = nullptr;
    bool data = 0;

public:
    AudioParameterCustomBool(bool* ptr_, const juce::String& parameterID, const juce::String& name, float defaultValue, const juce::String& label = juce::String()) : juce::AudioParameterBool(parameterID, name, defaultValue, label)
    {
        data = defaultValue;
        ptr = ptr_;
        type_index = std::type_index(typeid(bool));
    };
    bool shouldAutomate = false;
    operator bool() const noexcept { return AudioParameterBool::get(); }

    AudioParameterCustomBool& operator=(bool newValue)
    {
        (AudioParameterBool::operator=)(newValue);
        if (ptr)
        {
            data = newValue;
            *ptr = newValue;
        }
        return *this;
    }

    void update() override
    {
        if (ptr && *ptr != data)
        {
            this->operator=(*ptr);
        }
    }

    void update(void* ptr_) override
    {
        if (ptr == ptr_)
        {
            this->operator=(*ptr);
        }
    }

    bool isAutomatable() const override
    {
        return shouldAutomate;
    }
};

const juce::String paramX = "x";
const juce::String paramY = "y";
const juce::String paramZ = "3";

const juce::String paramRotation = "10";
const juce::String paramDiverge = "19";
const juce::String paramGain = "8";

const juce::String paramSTSpread = "9";
const juce::String paramSTRotate = "11";
const juce::String paramSTBalance = "12";
const juce::String paramAutoOrbit = "16";

const juce::String paramIsotropicEncode = "IsotropicEncode";
const juce::String paramPannerMode = "PannerMode";
const juce::String paramQuadMode = "QuadMode";
const juce::String paramSurroundMode = "SurroundMode";
//const String paramAmbiMode = "AmbiMode";
//const String paramOverlay = "Overlay";
const juce::String paramGhost = "Ghost";
const juce::String paramEqualPowerEncode = "EqualPowerEncode";

// Delay init
const juce::String paramITDActive = "ITDProcessing";
const juce::String paramDelayTime = "DelayTime";
const juce::String paramITDClampActive = "ITDClamp";
const juce::String paramDelayDistance = "ITDDistance";

AudioParameterCustomBool* bypassParameter = nullptr;
AudioParameterCustomFloat* xParameter = nullptr;
AudioParameterCustomFloat* yParameter = nullptr;
AudioParameterCustomFloat* zParameter = nullptr;
AudioParameterCustomFloat* gainParameter = nullptr;
AudioParameterCustomFloat* stereoSpreadParameter = nullptr;
AudioParameterCustomFloat* rotationParameter = nullptr;
AudioParameterCustomFloat* stereoOrbitAngleParameter = nullptr;
AudioParameterCustomFloat* stereoInputBalanceParameter = nullptr;
AudioParameterCustomBool* stereoAutoOrbitParameter = nullptr; // bool ?
AudioParameterCustomBool* isotropicEncodeParameter = nullptr; // bool ?
AudioParameterCustomFloat* pannerModeParameter = nullptr; // int ?
AudioParameterCustomFloat* quadModeParameter = nullptr; // int ?
AudioParameterCustomFloat* surroundModeParameter = nullptr; // int ?
AudioParameterCustomFloat* divergeParameter = nullptr;
AudioParameterCustomBool* ghostParameter = nullptr; // bool ?
AudioParameterCustomBool* equalPowerEncodeParameter = nullptr; // bool ?
AudioParameterCustomBool* itdParameter = nullptr;
AudioParameterCustomFloat* delayTimeParameter = nullptr;
AudioParameterCustomFloat* delayDistanceParameter = nullptr;

float getParameterFromValueTreeState(juce::AudioProcessorValueTreeState* mState, juce::String parameterIO)
{
    juce::NormalisableRange<float> range = mState->getParameterRange(parameterIO);
    return range.convertFrom0to1(mState->getParameter(parameterIO)->getValue());
}

void legacyParametersRecall(const void* data, int sizeInBytes, juce::AudioProcessor& processorToConnectTo)
{
    juce::MemoryInputStream input(data, sizeInBytes, false);
    input.setPosition(0);
    juce::ValueTree tree = juce::ValueTree::readFromStream(input);
    if (tree.isValid())
    {
        // load from tree

        // TODO: is this safe to create a temp timer? should we destroy it?
        juce::ScopedPointer<juce::AudioProcessorValueTreeState> mState = new juce::AudioProcessorValueTreeState(processorToConnectTo, new juce::UndoManager());

        mState->createAndAddParameter("empty parameter", "empty parameter", TRANS("empty parameter"), juce::NormalisableRange<float>(0.0f, 1.0f, 0.1), 0, nullptr, nullptr, false, /* automatable ?*/ false);
        mState->createAndAddParameter(paramX, "x", TRANS("X"), juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1), 0, nullptr, nullptr, false, /* automatable ?*/ false);
        mState->createAndAddParameter(paramY, "y", TRANS("Y"), juce::NormalisableRange<float>(-100.0f, 100.0f, 0.1), 70.7f, nullptr, nullptr, false, /* automatable ?*/ false);
        mState->createAndAddParameter(paramZ, "Up/Down", TRANS("Z"), juce::NormalisableRange<float>(-90.0f, 90.0f, 0.1), 0, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramGain, "Gain", TRANS("Gain"), juce::NormalisableRange<float>(-90.0f, 24.0f, 0.1), 6, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramSTSpread, "Spread", TRANS("Spread"), juce::NormalisableRange<float>(0.0f, 100.0f, 0.1), 50.0f, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramRotation, "Rotation", TRANS("Rotation"), juce::NormalisableRange<float>(-180.0f, 180.0f, 0.1), 0, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramDiverge, "Diverge", TRANS("Diverge"), juce::NormalisableRange<float>(-100.0f, 100.0, 0.01f), 50.0f, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramSTRotate, "Stereo Orbit Angle", TRANS("Angle"), juce::NormalisableRange<float>(-180.0f, 180.0f, 0.001f), 0, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramSTBalance, "Stereo Input Balance", TRANS("Balance"), juce::NormalisableRange<float>(-1.0f, 1.0f, 0.01), 0, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramAutoOrbit, "Stereo AutoOrbit", TRANS("AutoOrbit"), juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramIsotropicEncode, "Isotropic Encode", TRANS("IsotropicEncode"), juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramPannerMode, "PannerMode", TRANS("Panner Mode"), juce::NormalisableRange<float>(0.0f, 4.0f, 1.0f), 1.0f, nullptr, nullptr, false, /* automatable ?*/ false);
        mState->createAndAddParameter(paramQuadMode, "QuadMode", TRANS("QuadMode"), juce::NormalisableRange<float>(0.0f, 4.0f /*Max number of 4 channel input types, TODO: make var */, 1.0f), 0.0f, nullptr, nullptr, false, /* automatable ?*/ false);
        mState->createAndAddParameter(paramSurroundMode, "SurroundMode", TRANS("SurroundMode"), juce::NormalisableRange<float>(0.0f, 6.0f /*Max number of 4 channel input types, TODO: make var */, 1.0f), 0.0f, nullptr, nullptr, false, /* automatable ?*/ false);
        //            mState->createAndAddParameter(paramAmbiMode, "AmbiMode", TRANS("AmbiMode"), NormalisableRange<float>(0.0f, 1.0f /*Max number of 2 channel input types, TODO: make var */, 1.0f), 0.0f, nullptr, nullptr, false, /* automatable ?*/ false);
        mState->createAndAddParameter(paramGhost, "Ghost", TRANS("Ghost"), juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 1.0f, nullptr, nullptr, false, /* automatable ?*/ false);
        mState->createAndAddParameter(paramEqualPowerEncode, "EqualPower Encode", TRANS("EqualPowerEncode"), juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f, nullptr, nullptr, false, /* automatable ?*/ true);
        mState->createAndAddParameter(paramITDActive, "ITD Processing", TRANS("ITDProcessing"), juce::NormalisableRange<float>(0.0f, 1.0f, 1.0f), 0.0f, nullptr, nullptr, false, true);
        mState->createAndAddParameter(paramDelayTime, "Delay Time (max)", TRANS("DelayTime"), juce::NormalisableRange<float>(0.0f, 10000.0f, 1.0f), 600.0f, nullptr, nullptr, false, true);
        mState->createAndAddParameter(paramDelayDistance, "Delay Distance", TRANS("DelayDistance"), juce::NormalisableRange<float>(0.0f, 1000.0f, 0.1f), 1.0f, nullptr, nullptr, false, true);

        mState->state = juce::ValueTree(juce::Identifier("PannerParameters"));
        mState->state = tree;

        *xParameter = getParameterFromValueTreeState(mState, paramX);
        *yParameter = getParameterFromValueTreeState(mState, paramY);
        *zParameter = getParameterFromValueTreeState(mState, paramZ);
        *gainParameter = getParameterFromValueTreeState(mState, paramGain);
        *stereoSpreadParameter = getParameterFromValueTreeState(mState, paramSTSpread);
        *rotationParameter = getParameterFromValueTreeState(mState, paramRotation);
        *stereoOrbitAngleParameter = getParameterFromValueTreeState(mState, paramSTRotate);
        *stereoInputBalanceParameter = getParameterFromValueTreeState(mState, paramSTBalance);
        *stereoAutoOrbitParameter = getParameterFromValueTreeState(mState, paramAutoOrbit);
        *isotropicEncodeParameter = getParameterFromValueTreeState(mState, paramIsotropicEncode);
        //*pannerModeParameter = getParameterFromValueTreeState(mState, paramPannerMode);
        // TODO: Get QUAD mode and apply to new panner
        *quadModeParameter = getParameterFromValueTreeState(mState, paramQuadMode);
        //*surroundModeParameter = getParameterFromValueTreeState(mState, paramSurroundMode);
        *divergeParameter = getParameterFromValueTreeState(mState, paramDiverge);
        //*ghostParameter = getParameterFromValueTreeState(mState, paramGhost);
        *equalPowerEncodeParameter = getParameterFromValueTreeState(mState, paramEqualPowerEncode);
#ifdef ITD_PARAMETERS
        *itdParameter = getParameterFromValueTreeState(mState, paramITDActive);
        *delayTimeParameter = getParameterFromValueTreeState(mState, paramDelayTime);
        *delayDistanceParameter = getParameterFromValueTreeState(mState, paramDelayDistance);
#endif
    }
}
