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

    inline float getParameterFromValueTreeState(juce::AudioProcessorValueTreeState* mState, juce::String parameterIO)
    {
        juce::NormalisableRange<float> range = mState->getParameterRange(parameterIO);
        return range.convertFrom0to1(mState->getParameter(parameterIO)->getValue());
    }

    inline Mach1EncodeInputMode convertLegacyQuadMode(int quadMode)
    {
        switch(quadMode) {
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

        // Debug print the entire XML tree
        juce::String xmlString = root->createDocument("");
        DBG("XML Tree:\n" + xmlString);

        // Optionally, write to a file for easier inspection
        juce::File debugFile = juce::File::getSpecialLocation(juce::File::userDesktopDirectory).getChildFile("debug_xml_output.xml");
        debugFile.replaceWithText(xmlString);

        // Map XML elements to panner settings
        settings.x = root->getChildByName("param_" + LegacyParameters::paramX)->getDoubleAttribute("value", 0.0);
        settings.y = root->getChildByName("param_" + LegacyParameters::paramY)->getDoubleAttribute("value", 0.0);
        settings.azimuth = root->getChildByName("param_" + LegacyParameters::paramRotation)->getDoubleAttribute("value", 0.0);
        settings.elevation = root->getChildByName("param_" + LegacyParameters::paramZ)->getDoubleAttribute("value", 0.0);
        settings.diverge = root->getChildByName("param_" + LegacyParameters::paramDiverge)->getDoubleAttribute("value", 0.0);
        settings.gain = root->getChildByName("param_" + LegacyParameters::paramGain)->getDoubleAttribute("value", 0.0);
        settings.stereoOrbitAzimuth = root->getChildByName("param_" + LegacyParameters::paramSTRotate)->getDoubleAttribute("value", 0.0);
        settings.stereoSpread = root->getChildByName("param_" + LegacyParameters::paramSTSpread)->getDoubleAttribute("value", 0.0);
        settings.stereoInputBalance = root->getChildByName("param_" + LegacyParameters::paramSTBalance)->getDoubleAttribute("value", 0.0);
        settings.autoOrbit = root->getChildByName("param_" + LegacyParameters::paramAutoOrbit)->getBoolAttribute("value", false);
        settings.isotropicMode = root->getChildByName("param_" + LegacyParameters::paramIsotropicEncode)->getBoolAttribute("value", false);
        settings.equalpowerMode = root->getChildByName("param_" + LegacyParameters::paramEqualPowerEncode)->getBoolAttribute("value", false);

        // Handle quad mode conversion
        int quadMode = root->getChildByName("param_" + LegacyParameters::paramQuadMode)->getIntAttribute("value", 0);
        settings.m1Encode.setInputMode(convertLegacyQuadMode(quadMode));
    }

} // end of namespace LegacyParameters
