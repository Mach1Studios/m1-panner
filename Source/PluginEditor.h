/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/PannerUIBaseComponent.h"
#include "Overlay.h"

//==============================================================================
/**
*/
class M1PannerAudioProcessorEditor  : public juce::AudioProcessorEditor, public juce::Timer
{
public:
    M1PannerAudioProcessorEditor (M1PannerAudioProcessor &p, Mach1EncodePannerMode &pMode, Mach1EncodeInputModeType &iMode);
    ~M1PannerAudioProcessorEditor() override;
    
    M1PannerAudioProcessor* processor = nullptr;
    PannerUIBaseComponent* pannerUIBaseComponent = nullptr;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;
    bool isOverlayShow = false;
    
private:
    // This reference is provided as a quick way for your editor to
    // access the processor object that created it.
    
    // overlay
    std::unique_ptr<Overlay> overlayWindow;
    std::unique_ptr<OverlayDialogWindow> overlayDialogWindow;
    juce::DialogWindow::LaunchOptions overlayDialogLaunchOptions;
    
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (M1PannerAudioProcessorEditor)
};
