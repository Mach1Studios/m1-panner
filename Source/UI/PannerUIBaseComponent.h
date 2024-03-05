/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "juce_murka/JuceMurkaBaseComponent.h"

#include "../TypesForDataExchange.h"
#include "../PluginProcessor.h"
#include "../Config.h"

#include "M1Label.h"
#include "M1Knob.h"
#include "M1PitchWheel.h"
#include "M1VolumeDisplayLine.h"
#include "M1Checkbox.h"
#include "M1DropdownMenu.h"
#include "M1DropdownButton.h"
#include "PannerReticleField.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class PannerUIBaseComponent : public JuceMurkaBaseComponent
{
    M1PannerAudioProcessor* processor = nullptr;
	PannerSettings* pannerState = nullptr;
    MixerSettings* monitorState = nullptr;

public:
    //==============================================================================
    PannerUIBaseComponent(M1PannerAudioProcessor* processor);
	~PannerUIBaseComponent();

    //==============================================================================
    void initialise() override;
    void draw() override;
    //==============================================================================
    void paint (juce::Graphics& g) override;
    void resized() override;

	std::function<void(bool)> setOverlayVisible;

private:
	MurkaPoint cachedMousePositionWhenMouseWasHidden = { 0, 0 };
	MurkaPoint currentMousePositionJuceScaled = { 0, 0 };

	MurImage m1logo;

	bool pitchWheelHoveredAtLastFrame = false; // to tie up Z knob and pitch wheel
	bool reticleHoveredLastFrame = false;
	bool divergeKnobDraggingNow = false;
	bool rotateKnobDraggingNow = false;

	std::function<void()> cursorHide = [&]() {
		setMouseCursor(juce::MouseCursor::NoCursor);
		cachedMousePositionWhenMouseWasHidden = currentMousePositionJuceScaled;
	};
	std::function<void()> cursorShow = [&]() {
		setMouseCursor(juce::MouseCursor::NormalCursor);
	};
	std::function<void()> cursorShowAndTeleportBack = [&]() {
		setMouseCursor(juce::MouseCursor::NormalCursor);
        juce::Desktop::setMousePosition(localPointToGlobal(juce::Point<int>(cachedMousePositionWhenMouseWasHidden.x, cachedMousePositionWhenMouseWasHidden.y)));
	};
	std::function<void(int, int)> teleportCursor = [&](int x, int y) {
		//
	};
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PannerUIBaseComponent)
};
