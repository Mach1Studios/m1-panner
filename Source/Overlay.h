#pragma once

#include <JuceHeader.h>

#include "WindowUtil.h"

#include "UI/NativeOverlayUIComponent.h"

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
    NativeOverlayUIComponent* overlayUIBaseComponent = nullptr;

public:
    //==============================================================================
    Overlay(M1PannerAudioProcessor* processor, M1PannerAudioProcessorEditor* editor);
    ~Overlay();
    //==============================================================================

    void paint(juce::Graphics& g);
    void resized();

    void addOverlayComponent()
    {
        // ui component
        if (overlayUIBaseComponent == nullptr)
        {
            overlayUIBaseComponent = new NativeOverlayUIComponent(*processor);
            overlayUIBaseComponent->setBounds(getLocalBounds());
            addAndMakeVisible(overlayUIBaseComponent);
        }
    }

    void removeOverlayComponent()
    {
        if (overlayUIBaseComponent)
        {
            removeAllChildren();

            delete overlayUIBaseComponent;
            overlayUIBaseComponent = nullptr;
        }
    }

    void timerCallback();

    void setDialogWindow(juce::DialogWindow* dialogWindow);

    std::function<void(bool)> setOverlayVisible;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(Overlay)
};
