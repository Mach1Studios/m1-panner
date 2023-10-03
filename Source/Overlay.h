#pragma once

#include <JuceHeader.h>

#include "WindowUtil.h"

#include "UI/OverlayUIBaseComponent.h"

class M1PannerAudioProcessor;
class M1PannerAudioProcessorEditor;

class OverlayDialogWindow : public juce::DialogWindow
{
public:
    OverlayDialogWindow(LaunchOptions& options);

	void closeButtonPressed() override;

	M1PannerAudioProcessor* processor = nullptr;
    M1PannerAudioProcessorEditor* editor = nullptr;
};

class Overlay : public juce::Component, public juce::Timer
{
	M1PannerAudioProcessor* processor = nullptr;
    M1PannerAudioProcessorEditor* editor = nullptr;
	juce::DialogWindow* dialogWindow = nullptr;
	OverlayUIBaseComponent* overlayUIBaseComponent = nullptr;

public:
	//==============================================================================
	Overlay(M1PannerAudioProcessor* processor, M1PannerAudioProcessorEditor* editor);
	~Overlay();
	//==============================================================================

	void paint(juce::Graphics& g);
	void resized();

	void addOpenGLComponent() {
		// ui component
		if (overlayUIBaseComponent == nullptr) {
			overlayUIBaseComponent = new OverlayUIBaseComponent(processor);
			overlayUIBaseComponent->setSize(getWidth(), getHeight());
			addAndMakeVisible(overlayUIBaseComponent);
		}
	}

	void removeOpenGLComponent() {
		if (overlayUIBaseComponent) {
			overlayUIBaseComponent->shutdownOpenGL();
			removeAllChildren();

			delete overlayUIBaseComponent;
			overlayUIBaseComponent = nullptr;
		}

	}

	void timerCallback();

	void setDialogWindow(juce::DialogWindow* dialogWindow);

	std::function<void(bool)> setOverlayVisible;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Overlay)
};
