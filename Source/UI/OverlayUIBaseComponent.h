#pragma once

#include <JuceHeader.h>

#include "juce_murka/JuceMurkaBaseComponent.h"

#include "M1Knob.h"
#include "M1Label.h"

#include "../PluginProcessor.h"
#include "../TypesForDataExchange.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class OverlayUIBaseComponent : public JuceMurkaBaseComponent
{
    M1PannerAudioProcessor* processor = nullptr;
    PannerSettings* pannerState = nullptr;
    MixerSettings* monitorState = nullptr;

public:
    //==============================================================================
    OverlayUIBaseComponent(M1PannerAudioProcessor* processor);
    ~OverlayUIBaseComponent();

    //==============================================================================
    void initialise() override;
    void draw() override;

    void convertRCtoXYRaw(float r, float d, float& x, float& y);
    void convertXYtoRCRaw(float x, float y, float& r, float& d);
    //==============================================================================
    void paint(juce::Graphics& g) override;
    void resized() override;

    std::function<void(bool)> setOverlayVisible;

private:
    juce::Point<int> cachedMousePositionWhenMouseWasHidden = { 0, 0 };
    juce::Point<int> currentMousePosition = { 0, 0 };

    MurImage m1logo;

    bool pitchWheelHoveredAtLastFrame = false; // to tie up Z knob and pitch wheel
    bool reticleHoveredLastFrame = false;
    bool divergeKnobDraggingNow = false;
    bool rotateKnobDraggingNow = false;

    std::function<void()> cursorHide = [&]() {
        setMouseCursor(juce::MouseCursor::NoCursor);
        cachedMousePositionWhenMouseWasHidden = getLocalPoint(nullptr, juce::Desktop::getMousePosition());
    };
    std::function<void()> cursorShow = [&]() {
        setMouseCursor(juce::MouseCursor::NormalCursor);
    };
    std::function<void()> cursorShowAndTeleportBack = [&]() {
        setMouseCursor(juce::MouseCursor::NormalCursor);
        juce::Desktop::setMousePosition(localPointToGlobal(juce::Point<int>(cachedMousePositionWhenMouseWasHidden.x, cachedMousePositionWhenMouseWasHidden.y)));
    };
    std::function<void(int, int)> teleportCursor = [&](int x, int y) {
        juce::Desktop::setMousePosition(localPointToGlobal(juce::Point<int>(x, y)));
    };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(OverlayUIBaseComponent)
};
