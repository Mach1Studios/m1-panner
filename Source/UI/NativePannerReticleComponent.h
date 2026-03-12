#pragma once

#include <JuceHeader.h>

#include "../PluginProcessor.h"
#include "PannerUIHelpers.h"

class NativePannerReticleComponent : public juce::Component, private juce::Timer
{
public:
    explicit NativePannerReticleComponent(M1PannerAudioProcessor& processor);
    ~NativePannerReticleComponent() override;

    void paint(juce::Graphics& g) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;

    void setGuideLinesVisible(bool divergeVisible, bool rotateVisible);
    bool isReticleHoveredOrDragging() const;

private:
    void timerCallback() override;
    void startGesture();
    void endGesture();
    void updateFromPosition(juce::Point<float> localPosition, bool notifyHost);
    void drawMonitorYaw(juce::Graphics& g, juce::Rectangle<float> bounds) const;
    void drawAdditionalReticles(juce::Graphics& g, juce::Rectangle<float> bounds, bool reticleHovered) const;
    juce::Point<float> reticlePositionForBounds(juce::Rectangle<float> bounds) const;
    bool hitTestAdditionalReticles(juce::Point<float> point);

    M1PannerAudioProcessor& processor;
    PannerSettings& pannerState;
    MixerSettings& monitorState;

    bool dragging = false;
    bool hovered = false;
    bool drawDivergeGuideLine = false;
    bool drawRotateGuideLine = false;
};
