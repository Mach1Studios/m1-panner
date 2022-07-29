#ifndef __JUCE_HEADER_B57065F632DEFA52__
#define __JUCE_HEADER_B57065F632DEFA52__

#include "JuceHeader.h"

#include "WindowUtil.h"

#include "UI/OverlayUIBaseComponent.h"

class M1pannerAudioProcessor;
class M1PannerEditor;

class OverlayDialogWindow : public DialogWindow
{
public:
	OverlayDialogWindow(LaunchOptions& options);

	void closeButtonPressed() override;

	M1pannerAudioProcessor* processor = nullptr;
	M1PannerEditor* editor = nullptr;
};

class Overlay : public Component, public Timer
{
	M1pannerAudioProcessor* processor = nullptr;
	M1PannerEditor* editor = nullptr;
	juce::DialogWindow* dialogWindow = nullptr;

	OverlayUIBaseComponent* overlayUIBaseComponent = nullptr;

public:
	//==============================================================================
	Overlay(M1pannerAudioProcessor* processor, M1PannerEditor* editor);
	~Overlay();
	//==============================================================================

	void paint(Graphics& g);
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

#endif  
