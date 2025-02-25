#include "Overlay.h"

#include "PluginEditor.h"
#include "PluginProcessor.h"

Overlay::Overlay(M1PannerAudioProcessor* processor_, M1PannerAudioProcessorEditor* editor_)
{
    dialogWindow = nullptr;

    processor = processor_;
    editor = editor_;

    setAlwaysOnTop(true);
    setSize(300, 300);

    addOpenGLComponent();

    startTimer(50);
}

Overlay::~Overlay()
{
    stopTimer();

    removeOpenGLComponent();
}

//==============================================================================
void Overlay::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
}

void Overlay::timerCallback()
{
    if (dialogWindow && dialogWindow->isVisible())
    {
        if (!WindowUtil::isFound)
        {
            WindowUtil::update();
            if (WindowUtil::isFound)
            {
                dialogWindow->setTopLeftPosition(WindowUtil::x, WindowUtil::y);
                dialogWindow->setSize(WindowUtil::width, WindowUtil::height);
            }
        }
    }
    else
    {
        WindowUtil::isFound = false;
    }
}

void Overlay::resized()
{
    if (dialogWindow)
    {
        overlayUIBaseComponent->setSize(getWidth(), getHeight());
    }
}

void Overlay::setDialogWindow(juce::DialogWindow* dialogWindow)
{
    this->dialogWindow = dialogWindow;
}

OverlayDialogWindow::OverlayDialogWindow(LaunchOptions& options) : DialogWindow(options.dialogTitle, options.dialogBackgroundColour, options.escapeKeyTriggersCloseButton, true)
{
    // Set window style to have a visible border
    int styleFlags = DocumentWindow::closeButton | DocumentWindow::minimiseButton;

    setUsingNativeTitleBar(true);
    setTopLeftPosition(0, 0);
    setSize(200, 200);
    setTitleBarButtonsRequired(styleFlags, false);
    setTitleBarHeight(12); // Standard macOS title bar height
    setDropShadowEnabled(false);
    setAlwaysOnTop(true);

    // Enable resizing
    setResizable(true, true); // Allow resizing from any edge

    if (options.content.willDeleteObject())
    {
        setContentOwned(options.content.release(), true);
    }
    else
    {
        setContentNonOwned(options.content.release(), true);
    }

    centreAroundComponent(options.componentToCentreAround, getWidth(), getHeight());
}

void OverlayDialogWindow::closeButtonPressed()
{
    processor->pannerSettings.overlay = false;
    editor->isOverlayShow = false;
}
