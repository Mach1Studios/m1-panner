#include "NativeOverlayUIComponent.h"

#include "PannerUIHelpers.h"

NativeOverlayUIComponent::NativeOverlayUIComponent(M1PannerAudioProcessor& processorToUse)
    : processor(processorToUse), pannerState(processorToUse.pannerSettings), reticle(processorToUse)
{
    divergeKnob.setLookAndFeel(&lookAndFeel);
    divergeKnob.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    divergeKnob.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    divergeKnob.setRange(-100.0, 100.0, 0.1);
    divergeKnob.setDoubleClickReturnValue(true, 50.0);
    divergeKnob.onDragStart = [this]() {
        auto& params = processor.getValueTreeState();
        if (auto* diverge = params.getParameter(processor.paramDiverge)) {
            diverge->beginChangeGesture();
        }
        processor.divergeOwnedByUI = true;
    };
    divergeKnob.onDragEnd = [this]() {
        auto& params = processor.getValueTreeState();
        if (auto* diverge = params.getParameter(processor.paramDiverge)) {
            diverge->endChangeGesture();
        }
        processor.divergeOwnedByUI = false;
    };
    divergeKnob.onValueChange = [this]() {
        if (syncingFromState) {
            return;
        }

        pannerState.diverge = static_cast<float>(divergeKnob.getValue());
        float x = 0.0f;
        float y = 0.0f;
        PannerUIHelpers::convertRCtoXYRaw(pannerState.azimuth, static_cast<float>(divergeKnob.getValue()), x, y);
        pannerState.x = x;
        pannerState.y = y;

        auto& params = processor.getValueTreeState();
        if (auto* diverge = params.getParameter(processor.paramDiverge)) {
            diverge->setValueNotifyingHost(diverge->convertTo0to1(static_cast<float>(divergeKnob.getValue())));
        }
    };

    divergeLabel.setLookAndFeel(&lookAndFeel);
    divergeLabel.setFont(juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 3)));
    divergeLabel.setJustificationType(juce::Justification::centred);
    divergeLabel.setText("DIVERGE", juce::dontSendNotification);

    addAndMakeVisible(reticle);
    addAndMakeVisible(divergeKnob);
    addAndMakeVisible(divergeLabel);

    startTimerHz(20);
}

NativeOverlayUIComponent::~NativeOverlayUIComponent()
{
    divergeKnob.setLookAndFeel(nullptr);
}

void NativeOverlayUIComponent::timerCallback()
{
    syncingFromState = true;
    divergeKnob.setValue(pannerState.diverge, juce::dontSendNotification);
    syncingFromState = false;
    repaint();
}

void NativeOverlayUIComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colours::transparentBlack);
}

void NativeOverlayUIComponent::resized()
{
    reticle.setBounds(getLocalBounds());
    const auto knobBounds = juce::Rectangle<int>(getWidth() / 2 - 35, getHeight() - 90, 70, 70);
    divergeKnob.setBounds(knobBounds);
    divergeLabel.setBounds(knobBounds.translated(0, -22));
}
