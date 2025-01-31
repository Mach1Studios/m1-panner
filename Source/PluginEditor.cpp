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

    // Whenever the processor wants to post an alert, it calls postAlertToUI
    // which we forward to our pannerUIBaseComponent:
    processor->postAlertToUI = [this](const AlertData& alert)
    {
        if (pannerUIBaseComponent)
            pannerUIBaseComponent->postAlert(alert);
    };

    // overlay init
    {
        overlayWindow = std::make_unique<Overlay>(processor, this);

        overlayDialogLaunchOptions.dialogTitle = juce::String("Overlay");
        overlayDialogLaunchOptions.content.setOwned(overlayWindow.get());
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
    pannerUIBaseComponent = new PannerUIBaseComponent(processor);
    pannerUIBaseComponent->setOverlayVisible = [&](bool visible) {
        isOverlayShow = visible;
    };
    pannerUIBaseComponent->setSize(getWidth(), getHeight());
    addAndMakeVisible(pannerUIBaseComponent);

    startTimer(50);

    // Add this to flush stored alerts:
    for (auto& alert : processor->pendingAlerts) {
        pannerUIBaseComponent->postAlert(alert);
    }
    processor->pendingAlerts.clear();
}

M1PannerAudioProcessorEditor::~M1PannerAudioProcessorEditor()
{
    overlayWindow = nullptr;
    stopTimer();
    pannerUIBaseComponent->shutdownOpenGL();
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
    // This is generally where you'll want to lay out the positions of any
    // subcomponents in your editor..
}

void M1PannerAudioProcessorEditor::timerCallback()
{
    if (isOverlayShow && !overlayDialogWindow->isVisible())
    {
        overlayWindow->addOpenGLComponent();
        overlayDialogWindow->setVisible(true);
    }
    else if (!isOverlayShow && overlayDialogWindow->isVisible())
    {
        overlayDialogWindow->setVisible(false);
        overlayWindow->removeOpenGLComponent();
    }
}
