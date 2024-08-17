#include "PannerUIBaseComponent.h"

using namespace murka;

//==============================================================================
PannerUIBaseComponent::PannerUIBaseComponent(M1PannerAudioProcessor* processor_)
{
	// Make sure you set the size of the component after
    // you add any child components.
	setSize (getWidth(), getHeight());

	processor = processor_;
	pannerState = &processor->pannerSettings;
    monitorState = &processor->monitorSettings;
}

PannerUIBaseComponent::~PannerUIBaseComponent()
{
}

//==============================================================================
void PannerUIBaseComponent::initialise()
{
	JuceMurkaBaseComponent::initialise();
    m1logo.loadFromRawData(BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);
}

void PannerUIBaseComponent::draw()
{
    // This clears the context with our background.
    //juce::OpenGLHelpers::clear(juce::Colour(255.0, 198.0, 30.0));
    
    float scale = (float)openGLContext.getRenderingScale() * 0.7; // (Desktop::getInstance().getMainMouseSource().getScreenPosition().x / 300.0); //  0.7;

    if (scale != m.getScreenScale()) {
        m.setScreenScale(scale);
        m.updateFontsTextures(&m);
        m.clearFontsTextures();
    }

    currentMousePositionJuceScaled = m.mousePosition() / m.getScreenScale();

    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
    m.setColor(BACKGROUND_GREY);
	m.clear();
    
    m.setLineWidth(2);
    
	XYRD xyrd = { pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge };
    auto & reticleField = m.prepare<PannerReticleField>(MurkaShape(25, 30, 400, 400));
    reticleField.controlling(&xyrd);
    
    reticleField.cursorHide = cursorHide;
    reticleField.cursorShow = cursorShow;
    reticleField.teleportCursor = teleportCursor;
    reticleField.shouldDrawDivergeGuideLine = divergeKnobDraggingNow;
    reticleField.shouldDrawRotateGuideLine = rotateKnobDraggingNow;
    reticleField.pannerState = pannerState;
    reticleField.monitorState = monitorState;
    // TODO: look into this update; make the update related to the processor->parameterChanged() call to capture host side calls
    reticleField.m1encodeUpdate = [&]() {
        juce::AudioPlayHead::CurrentPositionInfo currentPosition;
        if (processor->getPlayHead() != nullptr) {
            if (!currentPosition.isPlaying) {
                processor->updateM1EncodePoints();
            }
        }
    };
    reticleField.isConnected = processor->pannerOSC.IsConnected();
    reticleField.draw();
    
    if (reticleField.results) {
        processor->convertXYtoRCRaw(pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
    }
    reticleHoveredLastFrame = reticleField.reticleHoveredLastFrame;

	// Changes the default knob reaction speed to mouse. The higher the slower.
	float knobSpeed = 250;
    
	int xOffset = 0;
	int yOffset = 499;
	int knobWidth = 70;
	int knobHeight = 87;
	int M1LabelOffsetX = 0;
    int M1LabelOffsetY = 25;

	// X
    auto& xKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 10, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerState->x);
    xKnob.rangeFrom = -100;
    xKnob.rangeTo = 100;
    xKnob.floatingPointPrecision = 1;
    xKnob.speed = knobSpeed;
    xKnob.defaultValue = 0;
    xKnob.isEndlessRotary = false;
    xKnob.enabled = true;
    xKnob.externalHover = reticleHoveredLastFrame;
    xKnob.cursorHide = cursorHide;
    xKnob.cursorShow = cursorShowAndTeleportBack;
    xKnob.draw();
    
    if (xKnob.changed) {
		// update this parameter here, notifying host
        processor->convertXYtoRCRaw(pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
	}
    
	m.setColor(ENABLED_PARAM);
    auto& xLabel = m.prepare<M1Label>(MurkaShape(xOffset + 10 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    xLabel.label = "X";
    xLabel.alignment = TEXT_CENTER;
    xLabel.enabled = true;
    xLabel.highlighted = xKnob.hovered || reticleHoveredLastFrame;
    xLabel.draw();

	// Y
    auto& yKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 100, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerState->y);
    yKnob.rangeFrom = -100;
    yKnob.rangeTo = 100;
    yKnob.floatingPointPrecision = 1;
    yKnob.speed = knobSpeed;
    yKnob.defaultValue = 70.7;
    yKnob.isEndlessRotary = false;
    yKnob.enabled = true;
    yKnob.externalHover = reticleHoveredLastFrame;
    yKnob.cursorHide = cursorHide;
    yKnob.cursorShow = cursorShowAndTeleportBack;
    yKnob.draw();
    
    if (yKnob.changed) {
        processor->convertXYtoRCRaw(pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
    }
    
	m.setColor(ENABLED_PARAM);
    auto& yLabel = m.prepare<M1Label>(MurkaShape(xOffset + 100 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    yLabel.label = "Y";
    yLabel.alignment = TEXT_CENTER;
    yLabel.enabled = true;
    yLabel.highlighted = yKnob.hovered || reticleHoveredLastFrame;
    yLabel.draw();

	// Azimuth / Rotation
    auto& azKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 190, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerState->azimuth);
    azKnob.rangeFrom = -180;
    azKnob.rangeTo = 180;
    azKnob.floatingPointPrecision = 1;
    azKnob.speed = knobSpeed;
    azKnob.defaultValue = 0;
    azKnob.isEndlessRotary = true;
    azKnob.enabled = true;
    azKnob.postfix = "º";
    azKnob.externalHover = reticleHoveredLastFrame;
    azKnob.cursorHide = cursorHide;
    azKnob.cursorShow = cursorShowAndTeleportBack;
    azKnob.draw();

    if (azKnob.changed) {
        processor->convertRCtoXYRaw(pannerState->azimuth, pannerState->diverge, pannerState->x, pannerState->y);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
    }
    
	rotateKnobDraggingNow = azKnob.draggingNow;
	m.setColor(ENABLED_PARAM);
    auto& azLabel = m.prepare<M1Label>(MurkaShape(xOffset + 190 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    azLabel.label = "AZIMUTH";
    azLabel.alignment = TEXT_CENTER;
    azLabel.enabled = true;
    azLabel.highlighted = azKnob.hovered || reticleHoveredLastFrame;
    azLabel.draw();

	// Diverge
    auto& dKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 280, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerState->diverge);
    dKnob.rangeFrom = -100;
    dKnob.rangeTo = 100;
    dKnob.floatingPointPrecision = 1;
    dKnob.speed = knobSpeed;
    dKnob.defaultValue = 50;
    dKnob.isEndlessRotary = false;
    dKnob.enabled = true;
    dKnob.externalHover = reticleHoveredLastFrame;
    dKnob.cursorHide = cursorHide;
    dKnob.cursorShow = cursorShowAndTeleportBack;
    dKnob.draw();
    
    if (dKnob.changed) {
        processor->convertRCtoXYRaw(pannerState->azimuth, pannerState->diverge, pannerState->x, pannerState->y);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
    }
    
	divergeKnobDraggingNow = dKnob.draggingNow;
	m.setColor(ENABLED_PARAM);
    auto& dLabel = m.prepare<M1Label>(MurkaShape(xOffset + 280 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    dLabel.label = "DIVERGE";
    dLabel.alignment = TEXT_CENTER;
    dLabel.enabled = true;
    dLabel.highlighted = dKnob.hovered || reticleHoveredLastFrame;
    dLabel.draw();

	// Gain
    auto& gKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 370, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerState->gain);
    gKnob.rangeFrom = -90;
    gKnob.rangeTo = 24;
    gKnob.postfix = "dB";
    gKnob.prefix = std::string(pannerState->gain > 0 ? "+" : "");
    gKnob.floatingPointPrecision = 1;
    gKnob.speed = knobSpeed;
    gKnob.defaultValue = 6;
    gKnob.isEndlessRotary = false;
    gKnob.enabled = true;
    gKnob.externalHover = false;
    gKnob.cursorHide = cursorHide;
    gKnob.cursorShow = cursorShowAndTeleportBack;
    gKnob.draw();
    
    if (gKnob.changed) {
        processor->parameterChanged(processor->paramGain, pannerState->gain);
    }

	m.setColor(ENABLED_PARAM);
    auto& gLabel = m.prepare<M1Label>(MurkaShape(xOffset + 370 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    gLabel.label = "GAIN";
    gLabel.alignment = TEXT_CENTER;
    gLabel.enabled = true;
    gLabel.highlighted = gKnob.hovered || reticleHoveredLastFrame;
    gLabel.draw();
    
    // Z
    auto& zKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 450, yOffset, knobWidth, knobHeight))
                                            .controlling(&pannerState->elevation);
    zKnob.rangeFrom = -90;
    zKnob.rangeTo = 90;
    zKnob.prefix = "";
    zKnob.postfix = "º";
    zKnob.floatingPointPrecision = 1;
    zKnob.speed = knobSpeed;
    zKnob.defaultValue = 0;
    zKnob.isEndlessRotary = false;
    zKnob.enabled = !(pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeAFormat || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAFUMA || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB2OAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB2OAFUMA || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB3OAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB3OAFUMA) /* Block Elevation rotations on some input modes */;
    zKnob.externalHover = pitchWheelHoveredAtLastFrame;
    zKnob.cursorHide = cursorHide;
    zKnob.cursorShow = cursorShowAndTeleportBack;
    zKnob.draw();
    
    if (zKnob.changed) {
        processor->parameterChanged(processor->paramElevation, pannerState->elevation);
    }

    bool zHovered = zKnob.hovered;

    m.setColor(ENABLED_PARAM);
    auto& zLabel = m.prepare<M1Label>(MurkaShape(xOffset + 450 + M1LabelOffsetX, yOffset - M1LabelOffsetY,
                                              knobWidth, knobHeight));
    zLabel.label = "Z";
    zLabel.alignment = TEXT_CENTER;
    zLabel.enabled = !(pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeAFormat || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAFUMA || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB2OAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB2OAFUMA || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB3OAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB3OAFUMA) /* Block Elevation rotations on some input modes */;
    zLabel.highlighted = zKnob.hovered || reticleHoveredLastFrame;
    zLabel.draw();

    /// Bottom Row of Parameters / Knobs
    
#ifdef CUSTOM_CHANNEL_LAYOUT
    // Single channel layout
    yOffset += 140;
#else
    if (!processor->hostType.isProTools() || // not pro tools
        (processor->hostType.isProTools() && // or is pro tools and is input 4 or 6
         (processor->getMainBusNumInputChannels() == 4 || processor->getMainBusNumInputChannels() == 6) || // or is pro tools and has a higher order output configuration
         (processor->getMainBusNumOutputChannels() > 8))) {
        yOffset += 140 - 10;
    } else {
        yOffset += 140;
    }

#endif
    
	// S Rotation
    auto& srKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 190, yOffset, knobWidth, knobHeight))
                                        .controlling(&pannerState->stereoOrbitAzimuth);
    srKnob.rangeFrom = -180;
    srKnob.rangeTo = 180;
    srKnob.prefix = "";
    srKnob.postfix = "º";
    srKnob.floatingPointPrecision = 1;
    srKnob.speed = knobSpeed;
    srKnob.defaultValue = 0;
    srKnob.isEndlessRotary = true;
    srKnob.enabled = ((pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo) && !pannerState->autoOrbit);
    srKnob.externalHover = false;
    srKnob.cursorHide = cursorHide;
    srKnob.cursorShow = cursorShowAndTeleportBack;
    srKnob.draw();
    
    if (srKnob.changed) {
        processor->parameterChanged(processor->paramStereoOrbitAzimuth, pannerState->stereoOrbitAzimuth);
    }

	m.setColor(ENABLED_PARAM);
    auto& srLabel = m.prepare<M1Label>(MurkaShape(xOffset + 190 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    srLabel.label = "S ROTATE";
    srLabel.alignment = TEXT_CENTER;
    srLabel.enabled = ((pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo) && !pannerState->autoOrbit);
    srLabel.highlighted = srKnob.hovered || reticleHoveredLastFrame;
    srLabel.draw();

	// S Spread

	// TODO didChangeOutsideThisThread ???
    auto& ssKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 280, yOffset, knobWidth, knobHeight))
                                    .controlling(&pannerState->stereoSpread);
    ssKnob.rangeFrom = 0.0;
    ssKnob.rangeTo = 100.0;
    ssKnob.prefix = "";
    ssKnob.postfix = "";
    ssKnob.floatingPointPrecision = 1;
    ssKnob.speed = knobSpeed;
    ssKnob.defaultValue = 50.;
    ssKnob.isEndlessRotary = false;
    ssKnob.enabled = (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo);
    ssKnob.externalHover = false;
    ssKnob.cursorHide = cursorHide;
    ssKnob.cursorShow = cursorShowAndTeleportBack;
    ssKnob.draw();
    
    if (ssKnob.changed) {
        processor->parameterChanged(processor->paramStereoSpread, pannerState->stereoSpread);
    }

	m.setColor(ENABLED_PARAM);
    auto& ssLabel = m.prepare<M1Label>(MurkaShape(xOffset + 280 - 2 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth + 10, knobHeight));
    ssLabel.label = "S SPREAD";
    ssLabel.alignment = TEXT_CENTER;
    ssLabel.enabled = (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo);
    ssLabel.highlighted = ssKnob.hovered || reticleHoveredLastFrame;
    ssLabel.draw();

	// S Pan

    auto& spKnob = m.prepare<M1Knob>(MurkaShape(xOffset + 370, yOffset, knobWidth, knobHeight))
                                            .controlling(&pannerState->stereoInputBalance);
    spKnob.rangeFrom = -1;
    spKnob.rangeTo = 1;
    spKnob.prefix = "";
    spKnob.postfix = "";
    spKnob.floatingPointPrecision = 1;
    spKnob.speed = knobSpeed;
    spKnob.defaultValue = 0;
    spKnob.isEndlessRotary = false;
    spKnob.enabled = (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo);
    spKnob.externalHover = false;
    spKnob.cursorHide = cursorHide;
    spKnob.cursorShow = cursorShowAndTeleportBack;
    spKnob.draw();
        
    if (spKnob.changed) {
        processor->parameterChanged(processor->paramStereoInputBalance, pannerState->stereoInputBalance);
    }

	m.setColor(ENABLED_PARAM);
    auto& spLabel = m.prepare<M1Label>(MurkaShape(xOffset + 370 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    spLabel.label = "S PAN";
    spLabel.alignment = TEXT_CENTER;
    spLabel.enabled = (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo);
    spLabel.highlighted = spKnob.hovered || reticleHoveredLastFrame;
    spLabel.draw();

	/// CHECKBOXES
	float checkboxSlotHeight = 28;
    
    auto& overlayCheckbox = m.prepare<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 0,
                                                200, 20 })
                                                .controlling(&pannerState->overlay)
                                                .withLabel("OVERLAY");
    overlayCheckbox.enabled = true;
    overlayCheckbox.draw();
    
    if (overlayCheckbox.changed) {
        setOverlayVisible(pannerState->overlay);
    }
        
    auto& isotropicCheckbox = m.prepare<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 1,
                                                200, 20 })
                                                .controlling(&pannerState->isotropicMode)
                                                .withLabel("ISOTROPIC");
    isotropicCheckbox.enabled = true;
    isotropicCheckbox.draw();

    auto& equalPowerCheckbox = m.prepare<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 2,
                                                200, 20 })
                                                .controlling(&pannerState->equalpowerMode)
                                                .withLabel("EQUALPOWER");
    equalPowerCheckbox.enabled = (pannerState->isotropicMode);
    equalPowerCheckbox.draw();

    if (isotropicCheckbox.changed || equalPowerCheckbox.changed) {
        if (isotropicCheckbox.checked) {
            if (equalPowerCheckbox.checked) {
                processor->parameterChanged(processor->paramIsotropicEncodeMode, true);
                processor->parameterChanged(processor->paramEqualPowerEncodeMode, true);
            } else {
                processor->parameterChanged(processor->paramIsotropicEncodeMode, true);
                processor->parameterChanged(processor->paramEqualPowerEncodeMode, false);
            }
        } else {
            processor->parameterChanged(processor->paramIsotropicEncodeMode, false);
            processor->parameterChanged(processor->paramEqualPowerEncodeMode, false);
        }
    }

    auto& autoOrbitCheckbox = m.prepare<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 3,
                                                200, 20 })
                                                .controlling(&pannerState->autoOrbit)
                                                .withLabel("AUTO ORBIT");
    autoOrbitCheckbox.enabled = (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo);
    autoOrbitCheckbox.draw();
    
    if (autoOrbitCheckbox.changed) {
        processor->parameterChanged(processor->paramAutoOrbit, pannerState->autoOrbit);
    }
    
    // Note: pitchwheel range in inverted to draw top down
    auto& pitchWheel = m.prepare<M1PitchWheel>({ 445, 30 - 10,
                                            80, 400 + 20 });
    pitchWheel.cursorHide = cursorHide;
    pitchWheel.cursorShow = cursorShow;
    pitchWheel.offset = 10.;
    pitchWheel.rangeFrom = 90.;
    pitchWheel.rangeTo = -90.;
    pitchWheel.enabled = !(pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeAFormat || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAFUMA || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB2OAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB2OAFUMA || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB3OAACN || pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeB3OAFUMA) /* Block Elevation rotations on some input modes */;
    pitchWheel.externalHovered = zHovered;
    pitchWheel.isConnected = processor->pannerOSC.IsConnected();
    pitchWheel.monitorState = monitorState;
    pitchWheel.dataToControl = &pannerState->elevation;
    pitchWheel.draw();
    
    if (pitchWheel.changed) {
        processor->parameterChanged(processor->paramElevation, pannerState->elevation);
    }
    
	pitchWheelHoveredAtLastFrame = pitchWheel.hovered;

	// Drawing volume meters
    if (processor->layoutCreated && processor->pannerSettings.m1Encode.getOutputChannelsCount() > 0) {
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE-3);
        auto outputChannelsCount = processor->pannerSettings.m1Encode.getOutputChannelsCount();
        
        int lines = int((outputChannelsCount - 1) / 8) + 1; // rounded int value of rows
        double lineHeight = 433.0 / double(lines);
        int cursorX = 0, cursorY = 0;
        
        for (int channelIndex = 0; channelIndex < processor->pannerSettings.m1Encode.getOutputChannelsCount(); channelIndex++) {
            
            // get the index order from the host
            int output_channel_reordered = processor->output_channel_indices[channelIndex];
            
            auto& volumeDisplayLine = m.prepare<M1VolumeDisplayLine>({ 555 + 15 * cursorX, 30 + cursorY * lineHeight, 10, lineHeight - 33 }).withVolume(processor->outputMeterValuedB[output_channel_reordered]).draw();
            m.setColor(LABEL_TEXT_COLOR);
            auto font = m.getCurrentFont();
            double singleDigitOffset = 0;
            if (channelIndex < 9) singleDigitOffset = 4;
            font->drawString(std::to_string(channelIndex + 1), 553 + 15 * cursorX + singleDigitOffset, (cursorY + 1) * lineHeight);
            
            cursorX ++;
            if (cursorX > 7) {
                cursorY += 1;
                cursorX = 0;
            }
		}
        
        for (int line = 0; line < lines; line++) {
            for (int i = 0; i <= 56/lines; i += 6) {
                float db = -i + 12;
                float y = i * 7;
                // Background line
                m.setColor(REF_LABEL_TEXT_COLOR);
                m.prepare<M1Label>({ 555 + 15 * (processor->pannerSettings.m1Encode.getOutputChannelsCount() == 4 ? 4 : 8), 30 + y - m.getCurrentFont()->getLineHeight() / 2, 30, 30 }).text( i != 100 ? std::to_string((int)db) : "dB" ).draw();
            }
        }
	}
	
    /// Bottom bar
#ifdef CUSTOM_CHANNEL_LAYOUT
    // Remove bottom bar for CUSTOM_CHANNEL_LAYOUT macro
#else
    if (!processor->hostType.isProTools() || // not pro tools
        (processor->hostType.isProTools() && // or is pro tools and is input 4 or 6
         (processor->getMainBusNumInputChannels() == 4 || processor->getMainBusNumInputChannels() == 6) || // or is pro tools and has a higher order output configuration
         (processor->getMainBusNumOutputChannels() > 8))) {

        // Show bottom bar
        m.setLineWidth(1);
        m.setColor(GRID_LINES_3_RGBA);
        m.drawLine(0, m.getSize().height()-36, m.getSize().width(), m.getSize().height()-36); // Divider line
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(0, m.getSize().height(), m.getSize().width(), 35); // bottom bar
        
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
        
        // skip drawing the input label if we only have the output dropdown available in PT
        if (!processor->hostType.isProTools() || // is not PT
            (processor->hostType.isProTools() && // or has an input dropdown in PT
             (processor->getMainBusNumInputChannels() == 4 || processor->getMainBusNumInputChannels() == 6))) {
            // Input Channel Mode Selector
            m.setColor(200, 255);
            auto& inputLabel = m.prepare<M1Label>(MurkaShape(m.getSize().width()/2 - 120 - 40, m.getSize().height() - 26, 60, 20));
            inputLabel.label = "IN";
            inputLabel.alignment = TEXT_CENTER;
            inputLabel.enabled = false;
            inputLabel.highlighted = false;
            inputLabel.draw();
        }
        
        std::string inputLabelText = "";
        // we do not protect from PT host here because it is expected to be checked before the GUI
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeMono) inputLabelText = "MONO ";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeStereo) inputLabelText = "STEREO";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeLCR) inputLabelText = "LCR ";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeQuad) inputLabelText = "QUAD ";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeLCRS) inputLabelText = "LCRS ";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeAFormat) inputLabelText = "AFORMAT";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputMode5dot0) inputLabelText = "5.0 Film";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputMode5dot1Film) inputLabelText = "5.1 Film";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputMode5dot1DTS) inputLabelText = "5.1 DTS";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputMode5dot1SMTPE) inputLabelText = "5.1 SMPTE";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAACN) inputLabelText = "1OA ACN";
        if (pannerState->m1Encode.getInputMode() == Mach1EncodeInputModeBFOAFUMA) inputLabelText = "1OA FuMa";

        // INPUT DROPDOWN
        int dropdownItemHeight = 20;
        
        if (processor->hostType.isProTools()) {
            // PT or other hosts that support multichannel and need selector dropdown for >4 channel modes
            
            /// INPUTS
            if (processor->getMainBusNumInputChannels() == 4) {
                // Displaying options only available as 4 channel INPUT
                // Dropdown is used for QUADMODE indication only
                auto& inputDropdownButton = m.prepare<M1DropdownButton>({  m.getSize().width()/2 - 60 - 40,
                    m.getSize().height() - 28,
                    80, 20 })
                .withLabel(inputLabelText).draw();
                std::vector<std::string> input_options = {"QUAD", "LCRS", "AFORMAT", "1OA-ACN", "1OA-FuMa"};
                auto& inputDropdownMenu = m.prepare<M1DropdownMenu>({  m.getSize().width()/2 - 60 - 40,
                    m.getSize().height() - 28 - input_options.size() * dropdownItemHeight,
                    80, input_options.size() * dropdownItemHeight })
                .withOptions(input_options);
                
                if (inputDropdownButton.pressed) {
                    inputDropdownMenu.open();
                    inputDropdownMenu.selectedOption = (int)processor->pannerSettings.m1Encode.getInputMode();
                }
                
                inputDropdownMenu.optionHeight = dropdownItemHeight;
                inputDropdownMenu.fontSize = 9;
                inputDropdownMenu.draw();
                
                if (inputDropdownMenu.changed) {
                    if (inputDropdownMenu.selectedOption == 0) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeQuad);
                    } else if (inputDropdownMenu.selectedOption == 1) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeLCRS);
                    } else if (inputDropdownMenu.selectedOption == 2) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeAFormat);
                    } else if (inputDropdownMenu.selectedOption == 3) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeBFOAACN);
                    } else if (inputDropdownMenu.selectedOption == 4) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeBFOAFUMA);
                    }
                    processor->parameterChanged(processor->paramInputMode, pannerState->m1Encode.getInputMode());
                }
            } else if (processor->getMainBusNumInputChannels() == 6) {
                // Displaying options only available as 4 channel INPUT
                // Dropdown is used for QUADMODE indication only
                auto& inputDropdownButton = m.prepare<M1DropdownButton>({  m.getSize().width()/2 - 60 - 40,
                    m.getSize().height() - 28,
                    80, 20 })
                .withLabel(inputLabelText).draw();
                std::vector<std::string> input_options = {"5.1 Film", "5.1 DTS", "5.1 SMPTE"};
                auto& inputDropdownMenu = m.prepare<M1DropdownMenu>({  m.getSize().width()/2 - 60 - 40,
                    m.getSize().height() - 28 - input_options.size() * dropdownItemHeight,
                    80, input_options.size() * dropdownItemHeight })
                .withOptions(input_options);
                
                if (inputDropdownButton.pressed) {
                    inputDropdownMenu.open();
                    inputDropdownMenu.selectedOption = (int)processor->pannerSettings.m1Encode.getInputMode();
                }
                
                inputDropdownMenu.optionHeight = dropdownItemHeight;
                inputDropdownMenu.fontSize = 9;
                inputDropdownMenu.draw();
                
                if (inputDropdownMenu.changed) {
                    if (inputDropdownMenu.selectedOption == 0) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputMode5dot1Film);
                    } else if (inputDropdownMenu.selectedOption == 1) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputMode5dot1DTS);
                    } else if (inputDropdownMenu.selectedOption == 2) {
                        pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputMode5dot1SMTPE);
                    }
                    processor->parameterChanged(processor->paramInputMode, pannerState->m1Encode.getInputMode());
                }
            }
        } else if (processor->hostType.isReaper() || processor->getMainBusNumInputChannels() > 2) {
            // Multichannel DAWs will scale the available inputs based on what the host offers
            auto& inputDropdownButton = m.prepare<M1DropdownButton>({  m.getSize().width()/2 - 60 - 40,
                                                                    m.getSize().height() - 28,
                                                                    80, 20 })
                                                        .withLabel(inputLabelText).draw();
            std::vector<std::string> input_options = {"MONO", "STEREO"};
            if (processor->getMainBusNumInputChannels() >= 3) input_options.push_back("LCR");
            if (processor->getMainBusNumInputChannels() >= 4) {
                input_options.push_back("QUAD");
                input_options.push_back("LCRS");
                input_options.push_back("AFORMAT");
            }
            if (processor->getMainBusNumInputChannels() >= 5) input_options.push_back("5.0 Film");
            if (processor->getMainBusNumInputChannels() >= 6) {
                input_options.push_back("5.1 Film");
                input_options.push_back("5.1 DTS");
                input_options.push_back("5.1 SMPTE");
            }
            if (processor->getMainBusNumInputChannels() >= 4) {
                input_options.push_back("1OA-ACN");
                input_options.push_back("1OA-FuMa");
            }

            auto& inputDropdownMenu = m.prepare<M1DropdownMenu>({  m.getSize().width()/2 - 60 - 40,
                                                                m.getSize().height() - 28 - input_options.size() * dropdownItemHeight,
                                                                80, input_options.size() * dropdownItemHeight })
                                                                .withOptions(input_options);

            if (inputDropdownButton.pressed) {
                inputDropdownMenu.open();
                inputDropdownMenu.selectedOption = (int)pannerState->m1Encode.getInputMode();
            }

            inputDropdownMenu.optionHeight = dropdownItemHeight;
            inputDropdownMenu.fontSize = 9;
            inputDropdownMenu.draw();

            if (inputDropdownMenu.changed) {
                pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType(inputDropdownMenu.selectedOption));
                processor->parameterChanged(processor->paramInputMode, Mach1EncodeInputModeType(inputDropdownMenu.selectedOption));
            }
        } else {
            // STEREO hosts here (by default)
            auto& inputDropdownButton = m.prepare<M1DropdownButton>({  m.getSize().width()/2 - 60 - 40,
                                                                    m.getSize().height() - 28,
                                                                    80, 20 })
                                                        .withLabel(inputLabelText).draw();
            std::vector<std::string> input_options = {"MONO", "STEREO"};
            auto& inputDropdownMenu = m.prepare<M1DropdownMenu>({  m.getSize().width()/2 - 60 - 40,
                                                                m.getSize().height() - 28 - input_options.size() * dropdownItemHeight,
                                                                80, input_options.size() * dropdownItemHeight })
                                                        .withOptions(input_options);

            if (inputDropdownButton.pressed) {
                inputDropdownMenu.open();
                inputDropdownMenu.selectedOption = (int)processor->pannerSettings.m1Encode.getInputMode();
            }

            inputDropdownMenu.optionHeight = dropdownItemHeight;
            inputDropdownMenu.fontSize = 9;
            inputDropdownMenu.draw();

            if (inputDropdownMenu.changed) {
                if (inputDropdownMenu.selectedOption == 0) {
					pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeMono);
                } else if (inputDropdownMenu.selectedOption == 1) {
                    pannerState->m1Encode.setInputMode(Mach1EncodeInputModeType::Mach1EncodeInputModeStereo);
                }
                processor->parameterChanged(processor->paramInputMode, pannerState->m1Encode.getInputMode());
			}
        }

        // OUTPUT DROPDOWN & LABELS
        /// -> label
        if (!processor->hostType.isProTools() || // is not PT
            (processor->hostType.isProTools() && // or has an input dropdown in PT
             (processor->getMainBusNumInputChannels() == 4 || processor->getMainBusNumInputChannels() == 6)) ||
            (processor->hostType.isProTools() && // or has an output dropdown in PT
             processor->getMainBusNumOutputChannels() > 8)) {
            //draw the arrow in PT when there is a dropdown
            m.setColor(200, 255);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
            auto& arrowLabel = m.prepare<M1Label>(MurkaShape(m.getSize().width()/2 - 20, m.getSize().height() - 26, 40, 20));
            arrowLabel.label = "-->";
            arrowLabel.alignment = TEXT_CENTER;
            arrowLabel.enabled = false;
            arrowLabel.highlighted = false;
            arrowLabel.draw();
        }
            
        if (!processor->hostType.isProTools() || // is not PT
            (processor->hostType.isProTools() && // or has an output dropdown in PT
             processor->getMainBusNumOutputChannels() > 8)) {
            // OUTPUT DROPDOWN or LABEL
            m.setColor(200, 255);
            m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
            auto& outputLabel = m.prepare<M1Label>(MurkaShape(m.getSize().width()/2 + 110, m.getSize().height() - 26, 60, 20));
            outputLabel.label = "OUT";
            outputLabel.alignment = TEXT_CENTER;
            outputLabel.enabled = false;
            outputLabel.highlighted = false;
            outputLabel.draw();
            
            auto outputType = pannerState->m1Encode.getOutputMode();            
            auto& outputDropdownButton = m.prepare<M1DropdownButton>({ m.getSize().width()/2 + 20, m.getSize().height() - 28,
                                                        80, 20 })
                                                        .withLabel(std::to_string(pannerState->m1Encode.getOutputChannelsCount())).draw();
            std::vector<std::string> output_options = {"M1Horizon-4", "M1Spatial-8"};
            // add the outputs based on discovered number of channels from host
            if (processor->external_spatialmixer_active || processor->getMainBusNumOutputChannels() >= 12) output_options.push_back("M1Spatial-12");
            if (processor->external_spatialmixer_active || processor->getMainBusNumOutputChannels() >= 14) output_options.push_back("M1Spatial-14");
            
            auto& outputDropdownMenu = m.prepare<M1DropdownMenu>({  m.getSize().width()/2 + 20,
                m.getSize().height() - 28 - output_options.size() * dropdownItemHeight,
                120, output_options.size() * dropdownItemHeight })
            .withOptions(output_options);
            if (outputDropdownButton.pressed) {
				switch (pannerState->m1Encode.getOutputChannelsCount()) {
					case 4:
						outputDropdownMenu.selectedOption = 0;
						break;
					case 8:
						outputDropdownMenu.selectedOption = 1;
						break;
					case 12:
						outputDropdownMenu.selectedOption = 2;
						break;
					case 14:
						outputDropdownMenu.selectedOption = 3;
						break;
					default:
						outputDropdownMenu.selectedOption = -1;
						break;
				}

                outputDropdownMenu.open();
            }
            
            outputDropdownMenu.optionHeight = dropdownItemHeight;
            outputDropdownMenu.fontSize = 9;
            outputDropdownMenu.draw();

            if (outputDropdownMenu.changed) {
                if (outputDropdownMenu.selectedOption == 0) {
					pannerState->m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Horizon_4);
                } else if (outputDropdownMenu.selectedOption == 1) {
                    pannerState->m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_8);
                } else if (outputDropdownMenu.selectedOption == 2) {
                    pannerState->m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_12);
                } else if (outputDropdownMenu.selectedOption == 3) {
                    pannerState->m1Encode.setOutputMode(Mach1EncodeOutputModeType::Mach1EncodeOutputModeM1Spatial_14);
                }
                processor->parameterChanged(processor->paramOutputMode, pannerState->m1Encode.getOutputMode());
            }
        } else {
            // PT & 4 channels
            // hide bottom bar
        }
    }
#endif // end of bottom bar macro check
    
    /// Panner label
    m.setColor(200, 255);
    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
#ifdef CUSTOM_CHANNEL_LAYOUT
    auto& pannerLabel = m.prepare<M1Label>(MurkaShape(m.getSize().width() - 100, m.getSize().height() - 30, 80, 20));
#else
    int labelYOffset;
    if (!processor->hostType.isProTools() || // not pro tools
        (processor->hostType.isProTools() && // or is pro tools and is input 4 or 6
         (processor->getMainBusNumInputChannels() == 4 || processor->getMainBusNumInputChannels() == 6) || // or is pro tools and has a higher order output configuration
         (processor->getMainBusNumOutputChannels() > 8))) {
        labelYOffset = 26;
    } else {
        labelYOffset = 30;
    }
    auto& pannerLabel = m.prepare<M1Label>(MurkaShape(m.getSize().width() - 100, m.getSize().height() - labelYOffset, 80, 20));
#endif
    pannerLabel.label = "PANNER";
    pannerLabel.alignment = TEXT_CENTER;
    pannerLabel.enabled = false;
    pannerLabel.highlighted = false;
    pannerLabel.draw();
    
    m.setColor(200, 255);
#ifdef CUSTOM_CHANNEL_LAYOUT
    m.drawImage(m1logo, 20, m.getSize().height() - 30, 161 / 3, 39 / 3);
#else
    if (!processor->hostType.isProTools() || // not pro tools
        (processor->hostType.isProTools() && // or is pro tools and is input 4 or 6
         (processor->getMainBusNumInputChannels() == 4 || processor->getMainBusNumInputChannels() == 6) || // or is pro tools and has a higher order output configuration
         (processor->getMainBusNumOutputChannels() > 8))) {
        m.drawImage(m1logo, 25, m.getSize().height() - labelYOffset, 161 / 3, 39 / 3);
    } else {
        m.drawImage(m1logo, 25, m.getSize().height() - 30, 161 / 3, 39 / 3);
    }
#endif
    
    // update the panner state if a user is interacting with the UI
    if (azLabel.highlighted || dLabel.highlighted || zLabel.highlighted || xLabel.highlighted || yLabel.highlighted || srLabel.highlighted || ssLabel.highlighted || spLabel.highlighted || gLabel.highlighted) {
        processor->pannerSettings.state = 2;
    } else {
        processor->pannerSettings.state = 1;
    }
}

//==============================================================================
void PannerUIBaseComponent::paint (juce::Graphics& g)
{
    // You can add your component specific drawing code here!
    // This will draw over the top of the openGL background.
}

void PannerUIBaseComponent::resized()
{
    // This is called when the PannerUIBaseComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
}
