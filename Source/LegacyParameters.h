/*
  ==============================================================================

    LegacyParameters.h

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

namespace LegacyParameters
{
    // Legacy parameter IDs (for backwards compatibility)
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
    const juce::String paramQuadMode = "QuadMode";
    const juce::String paramEqualPowerEncode = "EqualPowerEncode";

#ifdef ITD_PARAMETERS
    const juce::String paramITDActive = "ITDProcessing";
    const juce::String paramDelayTime = "DelayTime";
    const juce::String paramDelayDistance = "ITDDistance";
#endif

    inline float getParameterFromValueTreeState(juce::AudioProcessorValueTreeState* mState, juce::String parameterIO)
    {
        juce::NormalisableRange<float> range = mState->getParameterRange(parameterIO);
        return range.convertFrom0to1(mState->getParameter(parameterIO)->getValue());
    }

    inline Mach1EncodeInputMode convertLegacyQuadMode(int quadMode)
    {
        switch (quadMode)
        {
            case 0:  return Mach1EncodeInputMode::Quad;
            case 1:  return Mach1EncodeInputMode::LCRS;
            case 2:  return Mach1EncodeInputMode::AFormat;
            case 3:  return Mach1EncodeInputMode::BFOAACN;
            case 4:  return Mach1EncodeInputMode::BFOAFUMA;
            default: return Mach1EncodeInputMode::Quad;
        }
    }

    inline void updateFromLegacyXML(juce::XmlElement* root, PannerSettings& settings)
    {
        if (!root) return;

        // NOTE: Currently using `JUCE_FORCE_USE_LEGACY_PARAM_IDS=1` to ensure backwards compatibility.
        //       However, this may be removed in the future.

        // Debug print the entire XML tree
        juce::String xmlString = root->createDocument("");
        DBG("XML Tree:\n" + xmlString);

        // Map XML elements to panner settings
        // skipping X Y to avoid confusion with Azimuth/Diverge
        settings.azimuth = root->getChildByName("param_" + LegacyParameters::paramRotation)->getDoubleAttribute("value", settings.azimuth);
        settings.elevation = root->getChildByName("param_" + LegacyParameters::paramZ)->getDoubleAttribute("value", settings.elevation);
        settings.diverge = root->getChildByName("param_" + LegacyParameters::paramDiverge)->getDoubleAttribute("value", settings.diverge);
        settings.gain = root->getChildByName("param_" + LegacyParameters::paramGain)->getDoubleAttribute("value", settings.gain);
        settings.stereoOrbitAzimuth = root->getChildByName("param_" + LegacyParameters::paramSTRotate)->getDoubleAttribute("value", settings.stereoOrbitAzimuth);
        settings.stereoSpread = root->getChildByName("param_" + LegacyParameters::paramSTSpread)->getDoubleAttribute("value", settings.stereoSpread);
        settings.stereoInputBalance = root->getChildByName("param_" + LegacyParameters::paramSTBalance)->getDoubleAttribute("value", settings.stereoInputBalance);
        settings.autoOrbit = root->getChildByName("param_" + LegacyParameters::paramAutoOrbit)->getBoolAttribute("value", settings.autoOrbit);
        settings.isotropicMode = root->getChildByName("param_" + LegacyParameters::paramIsotropicEncode)->getBoolAttribute("value", settings.isotropicMode);
        settings.equalpowerMode = root->getChildByName("param_" + LegacyParameters::paramEqualPowerEncode)->getBoolAttribute("value", settings.equalpowerMode);

#ifdef ITD_PARAMETERS
        settings.itdActive = root->getChildByName("param_" + LegacyParameters::paramITDActive)->getBoolAttribute("value", settings.itdActive);
        settings.delayTime = root->getChildByName("param_" + LegacyParameters::paramDelayTime)->getDoubleAttribute("value", settings.delayTime);
        settings.delayDistance = root->getChildByName("param_" + LegacyParameters::paramDelayDistance)->getDoubleAttribute("value", settings.delayDistance);
#endif

        // Handle quad mode conversion
        int quadMode = root->getChildByName("param_" + LegacyParameters::paramQuadMode)->getIntAttribute("value", 0);
        settings.m1Encode.setInputMode(convertLegacyQuadMode(quadMode));
    }

} // end of namespace LegacyParameters
