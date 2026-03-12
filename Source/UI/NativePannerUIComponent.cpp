#include "NativePannerUIComponent.h"

#include <array>

#include "PannerUIHelpers.h"

namespace {

using namespace PannerUIHelpers;

constexpr int knobWidth = 70;
constexpr int knobHeight = 87;
constexpr int knobYOffset = 499;
constexpr int labelYOffset = 25;

juce::Colour rgb(unsigned char r, unsigned char g, unsigned char b)
{
    return juce::Colour::fromRGB(r, g, b);
}

juce::Colour rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return juce::Colour::fromRGBA(r, g, b, a);
}

} // namespace

NativePannerUIComponent::NativePannerUIComponent(M1PannerAudioProcessor& processorToUse)
    : processor(processorToUse), pannerState(processorToUse.pannerSettings), monitorState(processorToUse.monitorSettings), reticle(processorToUse)
{
    setOpaque(true);

    configureKnob(xKnob);
    configureKnob(yKnob);
    configureKnob(azimuthKnob);
    configureKnob(divergeKnob);
    configureKnob(gainKnob, " dB");
    configureKnob(elevationKnob);
    configureKnob(stereoRotateKnob);
    configureKnob(stereoSpreadKnob);
    configureKnob(stereoInputBalanceKnob);
    configureVerticalSlider(pitchSlider);

    xKnob.setRange(-100.0, 100.0, 0.1);
    yKnob.setRange(-100.0, 100.0, 0.1);
    azimuthKnob.setRange(-180.0, 180.0, 0.1);
    divergeKnob.setRange(-100.0, 100.0, 0.1);
    gainKnob.setRange(-90.0, 24.0, 0.1);
    elevationKnob.setRange(-90.0, 90.0, 0.1);
    stereoRotateKnob.setRange(-180.0, 180.0, 0.1);
    stereoSpreadKnob.setRange(0.0, 100.0, 0.1);
    stereoInputBalanceKnob.setRange(-1.0, 1.0, 0.01);
    pitchSlider.setRange(-90.0, 90.0, 1.0);

    xKnob.setDoubleClickReturnValue(true, 0.0);
    yKnob.setDoubleClickReturnValue(true, 70.7);
    azimuthKnob.setDoubleClickReturnValue(true, 0.0);
    divergeKnob.setDoubleClickReturnValue(true, 50.0);
    gainKnob.setDoubleClickReturnValue(true, 0.0);
    elevationKnob.setDoubleClickReturnValue(true, 0.0);
    stereoRotateKnob.setDoubleClickReturnValue(true, 0.0);
    stereoSpreadKnob.setDoubleClickReturnValue(true, 50.0);
    stereoInputBalanceKnob.setDoubleClickReturnValue(true, 0.0);

    configureLabel(xLabel, "X");
    configureLabel(yLabel, "Y");
    configureLabel(azimuthLabel, "ROTATE");
    configureLabel(divergeLabel, "DIVERGE");
    configureLabel(gainLabel, "GAIN");
    configureLabel(elevationLabel, "ELEVATE");
    configureLabel(stereoRotateLabel, "S-ROTATE");
    configureLabel(stereoSpreadLabel, "S-SPREAD");
    configureLabel(stereoInputBalanceLabel, "S-BAL");

    configureToggle(overlayToggle);
    configureToggle(isotropicToggle);
    configureToggle(equalPowerToggle);
    configureToggle(gainCompToggle);
    configureToggle(autoOrbitToggle);
    configureToggle(outputLockToggle);

    overlayToggle.setButtonText("OVERLAY");
    isotropicToggle.setButtonText("ISOTROPIC");
    equalPowerToggle.setButtonText("EQUALPOWER");
    gainCompToggle.setButtonText("GAIN COMP");
    autoOrbitToggle.setButtonText("AUTO ORBIT");
    outputLockToggle.setButtonText("LOCK");

    inputModeCombo.setLookAndFeel(&lookAndFeel);
    outputModeCombo.setLookAndFeel(&lookAndFeel);

    auto updateCartesian = [this]() {
        if (syncingFromState) {
            return;
        }

        pannerState.x = static_cast<float>(xKnob.getValue());
        pannerState.y = static_cast<float>(yKnob.getValue());

        float azimuth = 0.0f;
        float diverge = 0.0f;
        PannerUIHelpers::convertXYtoRCRaw(static_cast<float>(xKnob.getValue()), static_cast<float>(yKnob.getValue()), azimuth, diverge);
        pannerState.azimuth = azimuth;
        pannerState.diverge = diverge;
        setParamValue(processor.paramAzimuth, azimuth);
        setParamValue(processor.paramDiverge, diverge);
    };

    auto updatePolar = [this]() {
        if (syncingFromState) {
            return;
        }

        pannerState.azimuth = static_cast<float>(azimuthKnob.getValue());
        pannerState.diverge = static_cast<float>(divergeKnob.getValue());

        float x = 0.0f;
        float y = 0.0f;
        PannerUIHelpers::convertRCtoXYRaw(static_cast<float>(azimuthKnob.getValue()), static_cast<float>(divergeKnob.getValue()), x, y);
        pannerState.x = x;
        pannerState.y = y;
        setParamValue(processor.paramAzimuth, static_cast<float>(azimuthKnob.getValue()));
        setParamValue(processor.paramDiverge, static_cast<float>(divergeKnob.getValue()));
    };

    xKnob.onValueChange = updateCartesian;
    yKnob.onValueChange = updateCartesian;
    azimuthKnob.onValueChange = updatePolar;
    divergeKnob.onValueChange = updatePolar;

    gainKnob.onValueChange = [this]() {
        if (!syncingFromState) {
            pannerState.gain = static_cast<float>(gainKnob.getValue());
            setParamValue(processor.paramGain, static_cast<float>(gainKnob.getValue()));
        }
    };

    elevationKnob.onValueChange = [this]() {
        if (!syncingFromState) {
            pannerState.elevation = static_cast<float>(elevationKnob.getValue());
            setParamValue(processor.paramElevation, static_cast<float>(elevationKnob.getValue()));
        }
    };
    pitchSlider.onValueChange = [this]() {
        if (!syncingFromState) {
            pannerState.elevation = static_cast<float>(pitchSlider.getValue());
            setParamValue(processor.paramElevation, static_cast<float>(pitchSlider.getValue()));
        }
    };

    stereoRotateKnob.onValueChange = [this]() {
        if (!syncingFromState) {
            pannerState.stereoOrbitAzimuth = static_cast<float>(stereoRotateKnob.getValue());
            setParamValue(processor.paramStereoOrbitAzimuth, static_cast<float>(stereoRotateKnob.getValue()));
        }
    };
    stereoSpreadKnob.onValueChange = [this]() {
        if (!syncingFromState) {
            pannerState.stereoSpread = static_cast<float>(stereoSpreadKnob.getValue());
            setParamValue(processor.paramStereoSpread, static_cast<float>(stereoSpreadKnob.getValue()));
        }
    };
    stereoInputBalanceKnob.onValueChange = [this]() {
        if (!syncingFromState) {
            pannerState.stereoInputBalance = static_cast<float>(stereoInputBalanceKnob.getValue());
            setParamValue(processor.paramStereoInputBalance, static_cast<float>(stereoInputBalanceKnob.getValue()));
        }
    };

    for (auto* slider : { &xKnob, &yKnob, &azimuthKnob, &divergeKnob, &gainKnob, &elevationKnob, &stereoRotateKnob, &stereoSpreadKnob, &stereoInputBalanceKnob, &pitchSlider }) {
        slider->onDragStart = [this, slider]() {
            if (slider == &xKnob || slider == &yKnob || slider == &azimuthKnob || slider == &divergeKnob) {
                beginParamGesture(processor.paramAzimuth);
                beginParamGesture(processor.paramDiverge);
            } else if (slider == &gainKnob) {
                beginParamGesture(processor.paramGain);
            } else if (slider == &elevationKnob || slider == &pitchSlider) {
                beginParamGesture(processor.paramElevation);
            } else if (slider == &stereoRotateKnob) {
                beginParamGesture(processor.paramStereoOrbitAzimuth);
            } else if (slider == &stereoSpreadKnob) {
                beginParamGesture(processor.paramStereoSpread);
            } else if (slider == &stereoInputBalanceKnob) {
                beginParamGesture(processor.paramStereoInputBalance);
            }
        };

        slider->onDragEnd = [this, slider]() {
            if (slider == &xKnob || slider == &yKnob || slider == &azimuthKnob || slider == &divergeKnob) {
                endParamGesture(processor.paramAzimuth);
                endParamGesture(processor.paramDiverge);
            } else if (slider == &gainKnob) {
                endParamGesture(processor.paramGain);
            } else if (slider == &elevationKnob || slider == &pitchSlider) {
                endParamGesture(processor.paramElevation);
            } else if (slider == &stereoRotateKnob) {
                endParamGesture(processor.paramStereoOrbitAzimuth);
            } else if (slider == &stereoSpreadKnob) {
                endParamGesture(processor.paramStereoSpread);
            } else if (slider == &stereoInputBalanceKnob) {
                endParamGesture(processor.paramStereoInputBalance);
            }
        };
    }

    overlayToggle.onClick = [this]() {
        if (!syncingFromState) {
            pannerState.overlay = overlayToggle.getToggleState();
            if (setOverlayVisible) {
                setOverlayVisible(overlayToggle.getToggleState());
            }
            lastOverlayVisible = overlayToggle.getToggleState();
        }
    };
    isotropicToggle.onClick = [this]() { if (!syncingFromState) pannerState.isotropicMode = isotropicToggle.getToggleState(); };
    equalPowerToggle.onClick = [this]() { if (!syncingFromState) pannerState.equalpowerMode = equalPowerToggle.getToggleState(); };
    gainCompToggle.onClick = [this]() { if (!syncingFromState) pannerState.gainCompensationMode = gainCompToggle.getToggleState(); };
    autoOrbitToggle.onClick = [this]() { if (!syncingFromState) pannerState.autoOrbit = autoOrbitToggle.getToggleState(); };
    outputLockToggle.onClick = [this]() {
        if (syncingFromState) {
            return;
        }

        pannerState.lockOutputLayout = outputLockToggle.getToggleState();
        processor.parameterChanged("output_layout_lock", pannerState.lockOutputLayout);
        if (!outputLockToggle.getToggleState()) {
            processor.pannerOSC->sendRequestForCurrentChannelConfig();
        }
    };

    inputModeCombo.onChange = [this]() {
        if (syncingFromState || inputModeCombo.getSelectedItemIndex() < 0 || inputModeCombo.getSelectedItemIndex() >= static_cast<int>(inputModeOptions.size())) {
            return;
        }

        const auto mode = inputModeOptions[static_cast<size_t>(inputModeCombo.getSelectedItemIndex())].mode;
        pannerState.m1Encode.setInputMode(mode);
        setParamValue(processor.paramInputMode, static_cast<float>(mode));
        processor.needToUpdateM1EncodePoints = true;
        rebuildModeOptions();
    };

    outputModeCombo.onChange = [this]() {
        if (syncingFromState || outputModeCombo.getSelectedItemIndex() < 0 || outputModeCombo.getSelectedItemIndex() >= static_cast<int>(outputModeOptions.size())) {
            return;
        }

        const auto mode = outputModeOptions[static_cast<size_t>(outputModeCombo.getSelectedItemIndex())].mode;
        pannerState.m1Encode.setOutputMode(mode);
        setParamValue(processor.paramOutputMode, static_cast<float>(mode));
        processor.needToUpdateM1EncodePoints = true;
        rebuildModeOptions();
    };

    std::array<juce::Component*, 28> components = { &reticle, &xKnob, &yKnob, &azimuthKnob, &divergeKnob, &gainKnob, &elevationKnob,
                                                    &stereoRotateKnob, &stereoSpreadKnob, &stereoInputBalanceKnob, &pitchSlider, &xLabel, &yLabel, &azimuthLabel,
                                                    &divergeLabel, &gainLabel, &elevationLabel, &stereoRotateLabel, &stereoSpreadLabel, &stereoInputBalanceLabel,
                                                    &overlayToggle, &isotropicToggle, &equalPowerToggle, &gainCompToggle, &autoOrbitToggle, &outputLockToggle,
                                                    &inputModeCombo, &outputModeCombo };
    for (auto* component : components) {
        addAndMakeVisible(component);
    }

    rebuildModeOptions();
    syncFromState();
    startTimerHz(20);
}

NativePannerUIComponent::~NativePannerUIComponent()
{
    inputModeCombo.setLookAndFeel(nullptr);
    outputModeCombo.setLookAndFeel(nullptr);
}

void NativePannerUIComponent::configureKnob(juce::Slider& slider, const juce::String& suffix)
{
    slider.setLookAndFeel(&lookAndFeel);
    slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 60, 18);
    slider.setTextValueSuffix(suffix);
    slider.setNumDecimalPlacesToDisplay(1);
    slider.setPopupDisplayEnabled(false, false, this);
}

void NativePannerUIComponent::configureVerticalSlider(juce::Slider& slider, const juce::String& suffix)
{
    slider.setLookAndFeel(&lookAndFeel);
    slider.setSliderStyle(juce::Slider::LinearVertical);
    slider.setTextBoxStyle(juce::Slider::NoTextBox, false, 0, 0);
    slider.setTextValueSuffix(suffix);
}

void NativePannerUIComponent::configureToggle(juce::ToggleButton& toggle)
{
    toggle.setLookAndFeel(&lookAndFeel);
}

void NativePannerUIComponent::configureLabel(juce::Label& label, const juce::String& text)
{
    label.setLookAndFeel(&lookAndFeel);
    label.setText(text, juce::dontSendNotification);
    label.setFont(juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 3)));
    label.setJustificationType(juce::Justification::centred);
}

void NativePannerUIComponent::setParamValue(const juce::String& paramId, float value)
{
    auto& params = processor.getValueTreeState();
    if (auto* param = params.getParameter(paramId)) {
        param->setValueNotifyingHost(param->convertTo0to1(value));
    }
}

void NativePannerUIComponent::beginParamGesture(const juce::String& paramId)
{
    auto& params = processor.getValueTreeState();
    if (auto* param = params.getParameter(paramId)) {
        param->beginChangeGesture();
    }
}

void NativePannerUIComponent::endParamGesture(const juce::String& paramId)
{
    auto& params = processor.getValueTreeState();
    if (auto* param = params.getParameter(paramId)) {
        param->endChangeGesture();
    }
}

void NativePannerUIComponent::rebuildModeOptions()
{
    const auto inputChannels = processor.getMainBusNumInputChannels();
    inputModeOptions.clear();

    if (processor.hostType.isProTools() && inputChannels == 4) {
        inputModeOptions = { { "QUAD", Mach1EncodeInputMode::Quad },
                             { "LCRS", Mach1EncodeInputMode::LCRS },
                             { "AFORMAT", Mach1EncodeInputMode::AFormat },
                             { "1OA-ACN", Mach1EncodeInputMode::BFOAACN },
                             { "1OA-FuMa", Mach1EncodeInputMode::BFOAFUMA } };
    } else if (processor.hostType.isProTools() && inputChannels == 6) {
        inputModeOptions = { { "5.1 Film", Mach1EncodeInputMode::FiveDotOneFilm },
                             { "5.1 DTS", Mach1EncodeInputMode::FiveDotOneDTS },
                             { "5.1 SMPTE", Mach1EncodeInputMode::FiveDotOneSMTPE } };
    } else if (processor.hostType.isReaper() || inputChannels > 2) {
        inputModeOptions = { { "MONO", Mach1EncodeInputMode::Mono },
                             { "STEREO", Mach1EncodeInputMode::Stereo } };
        if (inputChannels >= 3) inputModeOptions.push_back({ "LCR", Mach1EncodeInputMode::LCR });
        if (inputChannels >= 4) {
            inputModeOptions.push_back({ "QUAD", Mach1EncodeInputMode::Quad });
            inputModeOptions.push_back({ "LCRS", Mach1EncodeInputMode::LCRS });
            inputModeOptions.push_back({ "AFORMAT", Mach1EncodeInputMode::AFormat });
            inputModeOptions.push_back({ "1OA-ACN", Mach1EncodeInputMode::BFOAACN });
            inputModeOptions.push_back({ "1OA-FuMa", Mach1EncodeInputMode::BFOAFUMA });
        }
        if (inputChannels >= 5) inputModeOptions.push_back({ "5.0 Film", Mach1EncodeInputMode::FiveDotZero });
        if (inputChannels >= 6) {
            inputModeOptions.push_back({ "5.1 Film", Mach1EncodeInputMode::FiveDotOneFilm });
            inputModeOptions.push_back({ "5.1 DTS", Mach1EncodeInputMode::FiveDotOneDTS });
            inputModeOptions.push_back({ "5.1 SMPTE", Mach1EncodeInputMode::FiveDotOneSMTPE });
        }
    } else {
        inputModeOptions = { { "MONO", Mach1EncodeInputMode::Mono }, { "STEREO", Mach1EncodeInputMode::Stereo } };
    }

    syncingFromState = true;
    inputModeCombo.clear(juce::dontSendNotification);
    int selectedInput = 1;
    for (size_t i = 0; i < inputModeOptions.size(); ++i) {
        inputModeCombo.addItem(inputModeOptions[i].label, static_cast<int>(i + 1));
        if (inputModeOptions[i].mode == pannerState.m1Encode.getInputMode()) {
            selectedInput = static_cast<int>(i + 1);
        }
    }
    inputModeCombo.setSelectedId(selectedInput, juce::dontSendNotification);

    outputModeOptions = { { "M1Horizon-4", Mach1EncodeOutputMode::M1Spatial_4 },
                          { "M1Spatial-8", Mach1EncodeOutputMode::M1Spatial_8 } };
    if (processor.external_spatialmixer_active || processor.getMainBusNumOutputChannels() >= 14) {
        outputModeOptions.push_back({ "M1Spatial-14", Mach1EncodeOutputMode::M1Spatial_14 });
    }

    outputModeCombo.clear(juce::dontSendNotification);
    int selectedOutput = 1;
    for (size_t i = 0; i < outputModeOptions.size(); ++i) {
        outputModeCombo.addItem(outputModeOptions[i].label, static_cast<int>(i + 1));
        if (outputModeOptions[i].mode == pannerState.m1Encode.getOutputMode()) {
            selectedOutput = static_cast<int>(i + 1);
        }
    }
    outputModeCombo.setSelectedId(selectedOutput, juce::dontSendNotification);
    syncingFromState = false;
}

void NativePannerUIComponent::syncFromState()
{
    syncingFromState = true;

    xKnob.setValue(pannerState.x, juce::dontSendNotification);
    yKnob.setValue(pannerState.y, juce::dontSendNotification);
    azimuthKnob.setValue(pannerState.azimuth, juce::dontSendNotification);
    divergeKnob.setValue(pannerState.diverge, juce::dontSendNotification);
    gainKnob.setValue(pannerState.gain, juce::dontSendNotification);
    elevationKnob.setValue(pannerState.elevation, juce::dontSendNotification);
    pitchSlider.setValue(pannerState.elevation, juce::dontSendNotification);
    stereoRotateKnob.setValue(pannerState.stereoOrbitAzimuth, juce::dontSendNotification);
    stereoSpreadKnob.setValue(pannerState.stereoSpread, juce::dontSendNotification);
    stereoInputBalanceKnob.setValue(pannerState.stereoInputBalance, juce::dontSendNotification);

    overlayToggle.setToggleState(pannerState.overlay, juce::dontSendNotification);
    isotropicToggle.setToggleState(pannerState.isotropicMode, juce::dontSendNotification);
    equalPowerToggle.setToggleState(pannerState.equalpowerMode, juce::dontSendNotification);
    gainCompToggle.setToggleState(pannerState.gainCompensationMode, juce::dontSendNotification);
    autoOrbitToggle.setToggleState(pannerState.autoOrbit, juce::dontSendNotification);
    outputLockToggle.setToggleState(pannerState.lockOutputLayout, juce::dontSendNotification);

    const auto inputMode = pannerState.m1Encode.getInputMode();
    const auto outputChannels = pannerState.m1Encode.getOutputChannelsCount();
    const bool elevationEnabled = PannerUIHelpers::supportsElevation(inputMode, outputChannels);
    const bool stereoEnabled = inputMode == Mach1EncodeInputMode::Stereo;

    elevationKnob.setEnabled(elevationEnabled);
    pitchSlider.setEnabled(elevationEnabled);
    stereoRotateKnob.setEnabled(stereoEnabled && !pannerState.autoOrbit);
    stereoSpreadKnob.setEnabled(stereoEnabled);
    stereoInputBalanceKnob.setEnabled(stereoEnabled);
    autoOrbitToggle.setEnabled(stereoEnabled);

    inputModeCombo.setTextWhenNothingSelected(PannerUIHelpers::inputModeLabel(inputMode));
    syncingFromState = false;

    reticle.setGuideLinesVisible(divergeKnob.isMouseButtonDown(), azimuthKnob.isMouseButtonDown());

    if (setOverlayVisible && lastOverlayVisible != overlayToggle.getToggleState()) {
        lastOverlayVisible = overlayToggle.getToggleState();
        setOverlayVisible(lastOverlayVisible);
    }
}

void NativePannerUIComponent::timerCallback()
{
    syncFromState();
}

void NativePannerUIComponent::layoutControlWithLabel(juce::Label& label, juce::Slider& slider, int x, int y, int w, int h, const juce::String& title)
{
    label.setText(title, juce::dontSendNotification);
    label.setBounds(scaledBounds(x, y - labelYOffset, w, 20));
    slider.setBounds(scaledBounds(x, y, w, h));
}

void NativePannerUIComponent::resized()
{
    reticle.setBounds(scaledBounds(25, 30, 400, 400));
    pitchSlider.setBounds(scaledBounds(445, 20, 80, 420));

    layoutControlWithLabel(xLabel, xKnob, 10, knobYOffset, knobWidth, knobHeight, "X");
    layoutControlWithLabel(yLabel, yKnob, 100, knobYOffset, knobWidth, knobHeight, "Y");
    layoutControlWithLabel(azimuthLabel, azimuthKnob, 190, knobYOffset, knobWidth, knobHeight, "ROTATE");
    layoutControlWithLabel(divergeLabel, divergeKnob, 280, knobYOffset, knobWidth, knobHeight, "DIVERGE");
    layoutControlWithLabel(gainLabel, gainKnob, 370, knobYOffset, knobWidth, knobHeight, "GAIN");
    layoutControlWithLabel(elevationLabel, elevationKnob, 450, knobYOffset, knobWidth, knobHeight, "ELEVATE");

    layoutControlWithLabel(stereoRotateLabel, stereoRotateKnob, 190, knobYOffset, knobWidth, knobHeight, "S-ROTATE");
    layoutControlWithLabel(stereoSpreadLabel, stereoSpreadKnob, 280, knobYOffset, knobWidth, knobHeight, "S-SPREAD");
    layoutControlWithLabel(stereoInputBalanceLabel, stereoInputBalanceKnob, 370, knobYOffset, knobWidth, knobHeight, "S-BAL");

    overlayToggle.setBounds(scaledBounds(557, 475, 140, 20));
    isotropicToggle.setBounds(scaledBounds(557, 495, 160, 20));
    equalPowerToggle.setBounds(scaledBounds(557, 515, 180, 20));
    gainCompToggle.setBounds(scaledBounds(557, 535, 180, 20));
    autoOrbitToggle.setBounds(scaledBounds(557, 448, 150, 20));

    inputModeCombo.setBounds(scaledBounds(261, 736, 80, 20));
    outputModeCombo.setBounds(scaledBounds(381, 736, 120, 20));
    outputLockToggle.setBounds(scaledBounds(540, 737, 80, 18));
}

void NativePannerUIComponent::paint(juce::Graphics& g)
{
    g.fillAll(rgb(BACKGROUND_GREY));

    g.setColour(rgba(GRID_LINES_3_RGBA));
    g.setFont(juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 3)));
    g.drawText("IN", scaledBounds(215, 739, 40, 20), juce::Justification::centred, false);
    g.drawText("OUT", scaledBounds(476, 739, 40, 20), juce::Justification::centred, false);
    g.drawText("-->", scaledBounds(331, 739, 40, 20), juce::Justification::centred, false);

    g.setColour(juce::Colour(200, 200, 200));
    g.drawText("PANNER", scaledBounds(620, 736, 80, 20), juce::Justification::centredRight, false);

    const auto meterBase = getLocalBounds().toFloat();
    const float meterScale = legacyUiScale;
    const float lineHeight = 26.0f * meterScale;
    const float barWidth = 10.0f * meterScale;
    const float meterStartX = 555.0f * meterScale;
    const float meterStartY = 30.0f * meterScale;

    for (int i = 0; i < processor.outputMeterValuedB.size(); ++i) {
        const auto x = meterStartX + 15.0f * meterScale * static_cast<float>(i);
        const auto meterBounds = juce::Rectangle<float>(x, meterStartY, barWidth, 350.0f * meterScale);
        g.setColour(juce::Colour(57, 57, 57));
        g.drawRect(meterBounds);

        const auto dB = processor.outputMeterValuedB[i];
        const auto normalized = juce::jlimit(0.0f, 1.0f, juce::jmap(dB, -60.0f, 6.0f, 0.0f, 1.0f));
        const auto fillBounds = meterBounds.withY(meterBounds.getBottom() - meterBounds.getHeight() * normalized)
                                            .withHeight(meterBounds.getHeight() * normalized);
        g.setColour(normalized > 0.85f ? rgb(METER_RED)
                                       : normalized > 0.6f ? rgb(METER_YELLOW)
                                                           : rgb(METER_GREEN));
        g.fillRect(fillBounds);
    }

    const bool stereoMode = pannerState.m1Encode.getInputMode() == Mach1EncodeInputMode::Stereo;
    stereoRotateKnob.setVisible(stereoMode);
    stereoRotateLabel.setVisible(stereoMode);
    stereoSpreadKnob.setVisible(stereoMode);
    stereoSpreadLabel.setVisible(stereoMode);
    stereoInputBalanceKnob.setVisible(stereoMode);
    stereoInputBalanceLabel.setVisible(stereoMode);

    xKnob.setVisible(!stereoMode);
    xLabel.setVisible(!stereoMode);
    yKnob.setVisible(!stereoMode);
    yLabel.setVisible(!stereoMode);
    azimuthKnob.setVisible(!stereoMode);
    azimuthLabel.setVisible(!stereoMode);
    divergeKnob.setVisible(!stereoMode);
    divergeLabel.setVisible(!stereoMode);
    gainKnob.setVisible(true);
    gainLabel.setVisible(true);
    elevationKnob.setVisible(true);
    elevationLabel.setVisible(true);
    autoOrbitToggle.setVisible(stereoMode);
}

void NativePannerUIComponent::postAlert(const Mach1::AlertData& alert)
{
    juce::NativeMessageBox::showMessageBoxAsync(juce::MessageBoxIconType::WarningIcon,
                                                alert.title,
                                                alert.message,
                                                this);
}
