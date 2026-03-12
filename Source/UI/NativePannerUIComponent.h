#pragma once

#include <JuceHeader.h>

#include "../AlertData.h"
#include "../PluginProcessor.h"
#include "NativeLookAndFeel.h"
#include "NativePannerReticleComponent.h"

class NativePannerUIComponent : public juce::Component, private juce::Timer
{
public:
    explicit NativePannerUIComponent(M1PannerAudioProcessor& processor);
    ~NativePannerUIComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

    void postAlert(const Mach1::AlertData& alert);

    std::function<void(bool)> setOverlayVisible;

private:
    struct InputModeOption {
        juce::String label;
        Mach1EncodeInputMode mode;
    };

    struct OutputModeOption {
        juce::String label;
        Mach1EncodeOutputMode mode;
    };

    void timerCallback() override;
    void configureKnob(juce::Slider& slider, const juce::String& suffix = {});
    void configureVerticalSlider(juce::Slider& slider, const juce::String& suffix = {});
    void configureToggle(juce::ToggleButton& toggle);
    void configureLabel(juce::Label& label, const juce::String& text);
    void rebuildModeOptions();
    void syncFromState();
    void layoutControlWithLabel(juce::Label& label, juce::Slider& slider, int x, int y, int w, int h, const juce::String& title);
    void setParamValue(const juce::String& paramId, float value);
    void beginParamGesture(const juce::String& paramId);
    void endParamGesture(const juce::String& paramId);

    M1PannerAudioProcessor& processor;
    PannerSettings& pannerState;
    MixerSettings& monitorState;
    NativePannerLookAndFeel lookAndFeel;
    NativePannerReticleComponent reticle;

    juce::Slider xKnob, yKnob, azimuthKnob, divergeKnob, gainKnob, elevationKnob;
    juce::Slider stereoRotateKnob, stereoSpreadKnob, stereoInputBalanceKnob, pitchSlider;
    juce::Label xLabel, yLabel, azimuthLabel, divergeLabel, gainLabel, elevationLabel;
    juce::Label stereoRotateLabel, stereoSpreadLabel, stereoInputBalanceLabel;
    juce::ToggleButton overlayToggle, isotropicToggle, equalPowerToggle, gainCompToggle, autoOrbitToggle, outputLockToggle;
    juce::ComboBox inputModeCombo, outputModeCombo;

    std::vector<InputModeOption> inputModeOptions;
    std::vector<OutputModeOption> outputModeOptions;
    bool syncingFromState = false;
    bool lastOverlayVisible = false;
};
