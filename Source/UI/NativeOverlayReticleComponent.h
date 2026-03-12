#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"

class NativeOverlayReticleComponent : public juce::Component, private juce::Timer
{
public:
    explicit NativeOverlayReticleComponent(M1PannerAudioProcessor& processor);
    ~NativeOverlayReticleComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;

private:
    void timerCallback() override;
    void beginGesture();
    void endGesture();
    void updateFromPosition(juce::Point<float> localPosition, bool notifyHost);
    juce::Point<float> reticlePositionForBounds(juce::Rectangle<float> bounds) const;

    M1PannerAudioProcessor& processor;
    PannerSettings& pannerState;
    MixerSettings& monitorState;
    bool dragging = false;
};
