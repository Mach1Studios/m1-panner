#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "NativeLookAndFeel.h"
#include "NativeOverlayReticleComponent.h"

class NativeOverlayUIComponent : public juce::Component, private juce::Timer
{
public:
    explicit NativeOverlayUIComponent(M1PannerAudioProcessor& processor);
    ~NativeOverlayUIComponent() override;

    void paint(juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    M1PannerAudioProcessor& processor;
    PannerSettings& pannerState;
    NativePannerLookAndFeel lookAndFeel;
    NativeOverlayReticleComponent reticle;
    juce::Slider divergeKnob;
    juce::Label divergeLabel;
    bool syncingFromState = false;
};
