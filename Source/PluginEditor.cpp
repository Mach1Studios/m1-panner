/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
M1PannerAudioProcessorEditor::M1PannerAudioProcessorEditor(M1PannerAudioProcessor& p)
    : AudioProcessorEditor(&p)
{
    // Make sure that before the constructor has finished, you've set the
    // editor's size to whatever you need it to be.
    setSize(505, 535);

    processor = &p;

    // overlay init
    {
        overlayWindow = std::make_unique<Overlay>(processor, this);

        overlayDialogLaunchOptions.dialogTitle = juce::String("Overlay");
        overlayDialogLaunchOptions.content.setNonOwned(overlayWindow.get());
        overlayDialogLaunchOptions.componentToCentreAround = this;
        overlayDialogLaunchOptions.escapeKeyTriggersCloseButton = true;
        overlayDialogLaunchOptions.useNativeTitleBar = false;
        overlayDialogLaunchOptions.resizable = true;
        overlayDialogLaunchOptions.useBottomRightCornerResizer = false;
#ifndef WIN32
        overlayDialogLaunchOptions.dialogBackgroundColour = juce::Colour(0x00FFFFFF);
#endif
        overlayDialogWindow = std::make_unique<OverlayDialogWindow>(overlayDialogLaunchOptions);
        overlayDialogWindow->processor = processor;
        overlayDialogWindow->editor = this;

        overlayWindow->setDialogWindow(overlayDialogWindow.get());
    }

    // ui component
    pannerUIBaseComponent = new NativePannerUIComponent(*processor);
    pannerUIBaseComponent->setOverlayVisible = [&](bool visible) {
        isOverlayShow = visible;
    };
    pannerUIBaseComponent->setSize(getWidth(), getHeight());
    addAndMakeVisible(pannerUIBaseComponent);

    auto safeThis = juce::Component::SafePointer<M1PannerAudioProcessorEditor>(this);
    processor->postAlertToUI = [safeThis, processor = processor](const Mach1::AlertData& alert)
    {
        if (safeThis != nullptr && safeThis->pannerUIBaseComponent != nullptr) {
            safeThis->pannerUIBaseComponent->postAlert(alert);
        } else if (processor != nullptr) {
            processor->pendingAlerts.push_back(alert);
        }
    };

    startTimer(50);

    // Add this to flush stored alerts:
    for (auto& alert : processor->pendingAlerts) {
        pannerUIBaseComponent->postAlert(alert);
    }
    processor->pendingAlerts.clear();
}

M1PannerAudioProcessorEditor::~M1PannerAudioProcessorEditor()
{
    if (processor != nullptr) {
        processor->postAlertToUI = nullptr;
    }
    overlayWindow = nullptr;
    stopTimer();
    removeAllChildren();
    delete pannerUIBaseComponent;
}

//==============================================================================
void M1PannerAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(40, 40, 40));
    g.setColour(juce::Colour(40, 40, 40));
}

void M1PannerAudioProcessorEditor::resized()
{
    if (pannerUIBaseComponent != nullptr) {
        pannerUIBaseComponent->setBounds(getLocalBounds());
    }
}

void M1PannerAudioProcessorEditor::timerCallback()
{
    if (isOverlayShow && !overlayDialogWindow->isVisible())
    {
        overlayWindow->addOverlayComponent();
        overlayDialogWindow->setVisible(true);
    }
    else if (!isOverlayShow && overlayDialogWindow->isVisible())
    {
        overlayDialogWindow->setVisible(false);
        overlayWindow->removeOverlayComponent();
    }
}
