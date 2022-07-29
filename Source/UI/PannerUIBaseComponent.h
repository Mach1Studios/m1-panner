/*
  ==============================================================================

    This file was auto-generated!

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>

#include "juce_murka/JuceMurkaBaseComponent.h"

#include "M1Knob.h"
#include "M1PitchWheel.h"
#include "M1VolumeDisplayLine.h"
#include "M1Checkbox.h"
#include "M1Label.h"

#include "../TypesForDataExchange.h"
#include "../PluginProcessor.h"

//==============================================================================
/*
    This component lives inside our window, and this is where you should put all
    your controls and content.
*/
class PannerUIBaseComponent : public JuceMurkaBaseComponent
{
	M1pannerAudioProcessor* processor = nullptr;
	PannerSettings* pannerSettings = nullptr;

public:
    //==============================================================================
    PannerUIBaseComponent(M1pannerAudioProcessor* processor);
	~PannerUIBaseComponent();

    //==============================================================================
    void initialise() override;
    void render() override;

	void convertRCtoXYRaw(float r, float d, float & x, float & y);
	void convertXYtoRCRaw(float x, float y, float &r, float &d);
    //==============================================================================
    void paint (Graphics& g) override;
    void resized() override;

	std::function<void(bool)> setOverlayVisible;

private:
	MurkaPoint cachedMousePositionWhenMouseWasHidden = { 0, 0 };
	MurkaPoint currentMousePosition = { 0, 0 };

	MurImage m1logo;

	bool pitchWheelHoveredAtLastFrame = false; // to tie up Z knob and pitch wheel
	bool reticleHoveredLastFrame = false;
	bool divergeKnobDraggingNow = false;
	bool rotateKnobDraggingNow = false;

	std::function<void()> cursorHide = [&]() {
		setMouseCursor(MouseCursor::NoCursor);
		cachedMousePositionWhenMouseWasHidden = currentMousePosition;
	};
	std::function<void()> cursorShow = [&]() {
		setMouseCursor(MouseCursor::NormalCursor);
	};
	std::function<void()> cursorShowAndTeleportBack = [&]() {
		setMouseCursor(MouseCursor::NormalCursor);
		Desktop::setMousePosition(localPointToGlobal(juce::Point<int>(cachedMousePositionWhenMouseWasHidden.x, cachedMousePositionWhenMouseWasHidden.y)));
	};
	std::function<void(int, int)> teleportCursor = [&](int x, int y) {
		//
	};

	MixerSettings mixerState;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PannerUIBaseComponent)
};
