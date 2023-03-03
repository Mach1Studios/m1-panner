#include "PannerUIBaseComponent.h"
#include "PannerReticleField.h"

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

struct Line2D {
	Line2D(double x, double y, double x2, double y2) : x{ x }, y{ y }, x2{ x2 }, y2{ y2 } {};

	MurkaPoint p() const {
		return { x, y };
	}

	MurkaPoint v() const {
		return { x2 - x, y2 - y };
	}

	double x, y, x2, y2;
};

/// A factor suitable to be passed to line \arg a as argument to calculate
/// the intersection point.
/// \NOTE A value in the range [0, 1] indicates a point between
/// a.p() and a.p() + a.v().
/// \NOTE The result is std::numeric_limits<double>::quiet_NaN() if the
/// lines do not intersect.
/// \SEE  intersection_point
inline double intersection(const Line2D& a, const Line2D& b) {
	const double Precision = std::sqrt(std::numeric_limits<double>::epsilon());
	double d = a.v().x * b.v().y - a.v().y * b.v().x;
	if (std::abs(d) < Precision) return std::numeric_limits<double>::quiet_NaN();
	else {
		double n = (b.p().x - a.p().x) * b.v().y
			- (b.p().y - a.p().y) * b.v().x;
		return n / d;
	}
}

/// The intersection of two lines.
/// \NOTE The result is a Point2D having the coordinates
///       std::numeric_limits<double>::quiet_NaN() if the lines do not
///       intersect.
inline MurkaPoint intersection_point(const Line2D& a, const Line2D& b) {
	// Line2D has an operator () (double r) returning p() + r * v()
	return a.p() + a.v() * (intersection(a, b));
}

void PannerUIBaseComponent::convertRCtoXYRaw(float r, float d, float &x, float &y) {
	x = cos(juce::degreesToRadians(-r + 90)) * d * sqrt(2);
	y = sin(juce::degreesToRadians(-r + 90)) * d * sqrt(2);
	if (x > 100) {
		auto intersection = intersection_point({ 0, 0, x, y },
			{ 100, -100, 100, 100 });
		x = intersection.x;
		y = intersection.y;
	}
	if (y > 100) {
		auto intersection = intersection_point({ 0, 0, x, y },
			{ -100, 100, 100, 100 });
		x = intersection.x;
		y = intersection.y;
	}
	if (x < -100) {
		auto intersection = intersection_point({ 0, 0, x, y },
			{ -100, -100, -100, 100 });
		x = intersection.x;
		y = intersection.y;
	}
	if (y < -100) {
		auto intersection = intersection_point({ 0, 0, x, y },
			{ -100, -100, 100, -100 });
		x = intersection.x;
		y = intersection.y;
	}
}

void PannerUIBaseComponent::convertXYtoRCRaw(float x, float y, float &r, float &d) {
	// TODO: issue with automating X Y and R C
    if (x == 0 && y == 0) {
		r = 0;
		d = 0;
	} else {
		d = sqrtf(x*x + y * y) / sqrt(2.0);
		float rotation_radian = atan2(x, y);//acos(x/d);
		r = juce::radiansToDegrees(rotation_radian);
	}
}

PannerUIBaseComponent::~PannerUIBaseComponent()
{
}

//==============================================================================
void PannerUIBaseComponent::initialise()
{
	JuceMurkaBaseComponent::initialise();

    std::string resourcesPath;
    if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
        resourcesPath = juce::File::getSpecialLocation(juce::File::SpecialLocationType::userApplicationDataDirectory).getChildFile("Application Support/Mach1 Spatial System/resources").getFullPathName().toStdString();
    } else {
        resourcesPath = juce::File::getSpecialLocation(juce::File::SpecialLocationType::userApplicationDataDirectory).getChildFile("Mach1 Spatial System/resources").getFullPathName().toStdString();
    }
    printf("Resources Loaded From: %s \n" , resourcesPath.c_str());
    m.setResourcesPath(resourcesPath);
    
    m1logo.loadFromRawData(BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);
}

void PannerUIBaseComponent::render()
{
    // This clears the context with our background.
    //juce::OpenGLHelpers::clear(juce::Colour(255.0, 198.0, 30.0));

    currentMousePosition = m.currentContext.mousePosition * 0.7;
    
    float scale = (float)openGLContext.getRenderingScale() * 0.7; // (Desktop::getInstance().getMainMouseSource().getScreenPosition().x / 300.0); //  0.7;

    if (scale != m.getScreenScale()) {
        m.setScreenScale(scale);
        m.updateFontsTextures(&m);
        m.clearFontsTextures();
    }

	m.begin();
    m.setFont("Proxima Nova Reg.ttf", 10);

	m.setColor(BACKGROUND_GREY);
	m.clear();
    
	XYRD xyrd = { pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge };
    auto & reticleField = m.drawWidget<PannerReticleField>(MurkaShape(25, 30, 400, 400));
    reticleField.controlling(&xyrd);
    
    reticleField.cursorHide = cursorHide;
    reticleField.cursorShow = cursorShow;
    reticleField.teleportCursor = teleportCursor;
    reticleField.shouldDrawDivergeGuideLine = divergeKnobDraggingNow;
    reticleField.shouldDrawRotateGuideLine = rotateKnobDraggingNow;
    reticleField.azimuth = pannerState->azimuth;
    reticleField.elevation = pannerState->elevation;
    reticleField.diverge = pannerState->diverge;
    reticleField.autoOrbit = pannerState->autoOrbit;
    reticleField.sRotate = pannerState->stereoOrbitAzimuth;
    reticleField.sSpread = pannerState->stereoSpread;
    reticleField.m1Encode = pannerState->m1Encode;
    reticleField.pannerState = pannerState;
    reticleField.monitorState = monitorState;
    reticleField.commit();
    
    if (reticleField.results) {
		convertXYtoRCRaw(pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
        processor->parameterChanged(processor->paramX, pannerState->x);
        processor->parameterChanged(processor->paramY, pannerState->y);
    }
    reticleHoveredLastFrame = reticleField.reticleHoveredLastFrame;

//    m.end();
//    return;

	m.setFont("Proxima Nova Reg.ttf", 10);

	// Changes the default knob reaction speed to mouse. The higher the slower.
	float knobSpeed = 350;
    
	int xOffset = 0;
	int yOffset = 499;
	int knobWidth = 70;
	int knobHeight = 87;
	int M1LabelOffsetX = 0;
    int M1LabelOffsetY = 25;

	// X
    auto& xKnob = m.draw<M1Knob>(MurkaShape(xOffset + 10, yOffset, knobWidth, knobHeight))
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
    xKnob.commit();
    
    if (xKnob.changed) {
		// update this parameter here, notifying host
		convertXYtoRCRaw(pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
        processor->parameterChanged(processor->paramX, pannerState->x);
        processor->parameterChanged(processor->paramY, pannerState->y);
	}
    
	m.setColor(ENABLED_PARAM);
    auto& xLabel = m.draw<M1Label>(MurkaShape(xOffset + 10 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    xLabel.label = "X";
    xLabel.alignment = TEXT_CENTER;
    xLabel.enabled = true;
    xLabel.highlighted = xKnob.hovered || reticleHoveredLastFrame;
    xLabel.commit();

	// Y
    auto& yKnob = m.draw<M1Knob>(MurkaShape(xOffset + 100, yOffset, knobWidth, knobHeight))
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
    yKnob.commit();
    
    if (yKnob.changed) {
        convertXYtoRCRaw(pannerState->x, pannerState->y, pannerState->azimuth, pannerState->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
        processor->parameterChanged(processor->paramX, pannerState->x);
        processor->parameterChanged(processor->paramY, pannerState->y);
    }
    
	m.setColor(ENABLED_PARAM);
    auto& yLabel = m.draw<M1Label>(MurkaShape(xOffset + 100 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    yLabel.label = "Y";
    yLabel.alignment = TEXT_CENTER;
    yLabel.enabled = true;
    yLabel.highlighted = yKnob.hovered || reticleHoveredLastFrame;
    yLabel.commit();

	// Azimuth / Rotation
    auto& azKnob = m.draw<M1Knob>(MurkaShape(xOffset + 190, yOffset, knobWidth, knobHeight))
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
    azKnob.commit();

    if (azKnob.changed) {
        convertRCtoXYRaw(pannerState->azimuth, pannerState->diverge, pannerState->x, pannerState->y);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
        processor->parameterChanged(processor->paramX, pannerState->x);
        processor->parameterChanged(processor->paramY, pannerState->y);
    }
    
	rotateKnobDraggingNow = azKnob.draggingNow;
	m.setColor(ENABLED_PARAM);
    auto& azLabel = m.draw<M1Label>(MurkaShape(xOffset + 190 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    azLabel.label = "AZIMUTH";
    azLabel.alignment = TEXT_CENTER;
    azLabel.enabled = true;
    azLabel.highlighted = azKnob.hovered || reticleHoveredLastFrame;
    azLabel.commit();

	// Diverge
    auto& dKnob = m.draw<M1Knob>(MurkaShape(xOffset + 280, yOffset, knobWidth, knobHeight))
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
    dKnob.commit();
    
    if (dKnob.changed) {
        convertRCtoXYRaw(pannerState->azimuth, pannerState->diverge, pannerState->x, pannerState->y);
        processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
        processor->parameterChanged(processor->paramX, pannerState->x);
        processor->parameterChanged(processor->paramY, pannerState->y);
    }
    
	divergeKnobDraggingNow = dKnob.draggingNow;
	m.setColor(ENABLED_PARAM);
    auto& dLabel = m.draw<M1Label>(MurkaShape(xOffset + 280 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    dLabel.label = "DIVERGE";
    dLabel.alignment = TEXT_CENTER;
    dLabel.enabled = true;
    dLabel.highlighted = dKnob.hovered || reticleHoveredLastFrame;
    dLabel.commit();

	// Gain
    auto& gKnob = m.draw<M1Knob>(MurkaShape(xOffset + 370, yOffset, knobWidth, knobHeight))
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
    gKnob.commit();
    
    if (gKnob.changed) {
        processor->parameterChanged(processor->paramGain, pannerState->gain);
    }

	m.setColor(ENABLED_PARAM);
    auto& gLabel = m.draw<M1Label>(MurkaShape(xOffset + 370 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    gLabel.label = "GAIN";
    gLabel.alignment = TEXT_CENTER;
    gLabel.enabled = true;
    gLabel.highlighted = gKnob.hovered || reticleHoveredLastFrame;
    gLabel.commit();
    
    // Z
    auto& zKnob = m.draw<M1Knob>(MurkaShape(xOffset + 450, yOffset, knobWidth, knobHeight))
                                            .controlling(&pannerState->elevation);
    zKnob.rangeFrom = -90;
    zKnob.rangeTo = 90;
    zKnob.prefix = "";
    zKnob.postfix = "º";
    zKnob.floatingPointPrecision = 1;
    zKnob.speed = knobSpeed;
    zKnob.defaultValue = 0;
    zKnob.isEndlessRotary = false;
    zKnob.enabled = true;
    zKnob.externalHover = pitchWheelHoveredAtLastFrame;
    zKnob.cursorHide = cursorHide;
    zKnob.cursorShow = cursorShowAndTeleportBack;
    zKnob.commit();
    
    if (zKnob.changed) {
        processor->parameterChanged(processor->paramElevation, pannerState->elevation);
    }

    bool zHovered = zKnob.hovered;

    m.setColor(ENABLED_PARAM);
    auto& zLabel = m.draw<M1Label>(MurkaShape(xOffset + 450 + M1LabelOffsetX, yOffset - M1LabelOffsetY,
                                              knobWidth, knobHeight));
    zLabel.label = "Z";
    zLabel.alignment = TEXT_CENTER;
    zLabel.enabled = true;
    zLabel.highlighted = zKnob.hovered || reticleHoveredLastFrame;
    zLabel.commit();

    /// Bottom Row of Parameters / Knobs
    
#ifdef CUSTOM_CHANNEL_LAYOUT
    // Single channel layout
    yOffset += 140;
#else
    if (processor->hostType.isProTools()) { // remove I/O bar for PT
        if (processor->getMainBusNumInputChannels() >= 4) { // unless we need space for the input dropdown menu
            yOffset += 140 - 10;
        } else {
            yOffset += 140;
        }
    } else { // show dropdown menus for i/o for all other hosts
        yOffset += 140 - 10;
    }
#endif
    
	// S Rotation
    auto& srKnob = m.draw<M1Knob>(MurkaShape(xOffset + 190, yOffset, knobWidth, knobHeight))
                                        .controlling(&pannerState->stereoOrbitAzimuth);
    srKnob.rangeFrom = -180;
    srKnob.rangeTo = 180;
    srKnob.prefix = "";
    srKnob.postfix = "º";
    srKnob.floatingPointPrecision = 1;
    srKnob.speed = knobSpeed;
    srKnob.defaultValue = 0;
    srKnob.isEndlessRotary = true;
    srKnob.enabled = ((pannerState->m1Encode->getInputMode() != Mach1EncodeInputModeMono) && !pannerState->autoOrbit);
    srKnob.externalHover = false;
    srKnob.cursorHide = cursorHide;
    srKnob.cursorShow = cursorShowAndTeleportBack;
    srKnob.commit();
    
    if (srKnob.changed) {
        processor->parameterChanged(processor->paramStereoOrbitAzimuth, pannerState->stereoOrbitAzimuth);
    }

	m.setColor(ENABLED_PARAM);
    auto& srLabel = m.draw<M1Label>(MurkaShape(xOffset + 190 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    srLabel.label = "S ROTATE";
    srLabel.alignment = TEXT_CENTER;
    srLabel.enabled = ((pannerState->m1Encode->getInputMode() != Mach1EncodeInputModeMono) && !pannerState->autoOrbit);
    srLabel.highlighted = srKnob.hovered || reticleHoveredLastFrame;
    srLabel.commit();

	// S Spread

	// TODO didChangeOutsideThisThread ???
    auto& ssKnob = m.draw<M1Knob>(MurkaShape(xOffset + 280, yOffset, knobWidth, knobHeight))
                                    .controlling(&pannerState->stereoSpread);
    ssKnob.rangeFrom = 0.0;
    ssKnob.rangeTo = 100.0;
    ssKnob.prefix = "";
    ssKnob.postfix = "";
    ssKnob.floatingPointPrecision = 1;
    ssKnob.speed = knobSpeed;
    ssKnob.defaultValue = 50.;
    ssKnob.isEndlessRotary = false;
    ssKnob.enabled = (pannerState->m1Encode->getInputMode() != Mach1EncodeInputModeMono);
    ssKnob.externalHover = false;
    ssKnob.cursorHide = cursorHide;
    ssKnob.cursorShow = cursorShowAndTeleportBack;
    ssKnob.commit();
    
    if (ssKnob.changed) {
        processor->parameterChanged(processor->paramStereoSpread, pannerState->stereoSpread);
    }

	m.setColor(ENABLED_PARAM);
    auto& ssLabel = m.draw<M1Label>(MurkaShape(xOffset + 280 - 2 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth + 10, knobHeight));
    ssLabel.label = "S SPREAD";
    ssLabel.alignment = TEXT_CENTER;
    ssLabel.enabled = (pannerState->m1Encode->getInputMode() != Mach1EncodeInputModeMono);
    ssLabel.highlighted = ssKnob.hovered || reticleHoveredLastFrame;
    ssLabel.commit();

	// S Pan

    auto& spKnob = m.draw<M1Knob>(MurkaShape(xOffset + 370, yOffset, knobWidth, knobHeight))
                                            .controlling(&pannerState->stereoInputBalance);
    spKnob.rangeFrom = -1;
    spKnob.rangeTo = 1;
    spKnob.prefix = "";
    spKnob.postfix = "";
    spKnob.floatingPointPrecision = 1;
    spKnob.speed = knobSpeed;
    spKnob.defaultValue = 0;
    spKnob.isEndlessRotary = false;
    spKnob.enabled = (pannerState->m1Encode->getInputMode() != Mach1EncodeInputModeMono);
    spKnob.externalHover = false;
    spKnob.cursorHide = cursorHide;
    spKnob.cursorShow = cursorShowAndTeleportBack;
    spKnob.commit();
        
    if (spKnob.changed) {
        processor->parameterChanged(processor->paramStereoInputBalance, pannerState->stereoInputBalance);
    }

	m.setColor(ENABLED_PARAM);
    auto& spLabel = m.draw<M1Label>(MurkaShape(xOffset + 370 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    spLabel.label = "S PAN";
    spLabel.alignment = TEXT_CENTER;
    spLabel.enabled = (pannerState->m1Encode->getInputMode() != Mach1EncodeInputModeMono);
    spLabel.highlighted = spKnob.hovered || reticleHoveredLastFrame;
    spLabel.commit();

	/// CHECKBOXES
	float checkboxSlotHeight = 28;
    
    auto& overlayCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 0,
                                                200, 20 })
                                                .controlling(&pannerState->overlay)
                                                .withLabel("OVERLAY");
    overlayCheckbox.enabled = true;
    overlayCheckbox.commit();
    
    if (overlayCheckbox.changed) {
        setOverlayVisible(pannerState->overlay);
    }
        
    auto& isotropicCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 1,
                                                200, 20 })
                                                .controlling(&pannerState->isotropicMode)
                                                .withLabel("ISOTROPIC");
    isotropicCheckbox.enabled = true;
    isotropicCheckbox.commit();

    auto& equalPowerCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 2,
                                                200, 20 })
                                                .controlling(&pannerState->equalpowerMode)
                                                .withLabel("EQUALPOWER");
    equalPowerCheckbox.enabled = (pannerState->isotropicMode);
    equalPowerCheckbox.commit();

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

    auto& autoOrbitCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 3,
                                                200, 20 })
                                                .controlling(&pannerState->autoOrbit)
                                                .withLabel("AUTO ORBIT");
    autoOrbitCheckbox.enabled = (pannerState->m1Encode->getInputMode() != Mach1EncodeInputModeMono);
    autoOrbitCheckbox.commit();
    
    if (autoOrbitCheckbox.changed) {
        processor->parameterChanged(processor->paramAutoOrbit, pannerState->autoOrbit);
    }
    
	//TODO: why are pitch ranges inversed?
    
    auto& pitchWheel = m.draw<M1PitchWheel>({ 445, 30 - 10,
                                            80, 400 + 20 });
    pitchWheel.cursorHide = cursorHide;
    pitchWheel.cursorShow = cursorShow;
    pitchWheel.offset = 10.;
    pitchWheel.rangeFrom = 90.;
    pitchWheel.rangeTo = -90.;
    pitchWheel.externalHovered = zHovered;
    pitchWheel.mixerPitch = monitorState->pitch;
    pitchWheel.dataToControl = &pannerState->elevation;
    pitchWheel.commit();
    
    if (pitchWheel.changed) {
        processor->parameterChanged(processor->paramElevation, pannerState->elevation);
    }
    
	pitchWheelHoveredAtLastFrame = pitchWheel.hovered;

	// Drawing volume meters
	if (processor->m1Encode.getOutputChannelsCount() > 0) {
        m.setFont("Proxima Nova Reg.ttf", 7);
		for (int channelIndex = 0; channelIndex < processor->m1Encode.getOutputChannelsCount(); channelIndex++) {
            auto& volumeDisplayLine = m.draw<M1VolumeDisplayLine>({ 555 + 15 * channelIndex, 30, 10, 400 }).withVolume(processor->outputMeterValuedB[channelIndex]).commit();

            m.setColor(LABEL_TEXT_COLOR);
            m.draw<M1Label>({ 555 + 15 * channelIndex, 433, 60, 50 }).text(std::to_string(channelIndex + 1)).commit();
		}

		m.setColor(REF_LABEL_TEXT_COLOR);
		for (int i = 0; i <= 56; i += 6) {
			//float db = ofMap(i, 0, 100, 0, -144); // 144 from M1VolumeDisplayLine math
			float db = -i + 12;

			float y = i * 7;
            // Background line
            m.draw<M1Label>({ 555 + 15 * processor->m1Encode.getOutputChannelsCount(), 30 + y - m.getCurrentFont()->getLineHeight() / 2,
                30, 30 }).text( i != 100 ? std::to_string((int)db) : "dB" ).commit();
		}
	}
	
    /// Bottom bar
#ifdef CUSTOM_CHANNEL_LAYOUT
    // Remove bottom bar for CUSTOM_CHANNEL_LAYOUT macro
#else
    if (!processor->hostType.isProTools() || (processor->hostType.isProTools() && processor->getMainBusNumInputChannels() >= 4)) {
        
        // Show bottom bar
        
        m.setColor(GRID_LINES_3_RGBA);
        m.drawLine(0, m.getSize().height()-36, m.getSize().width(), m.getSize().height()-36); // Divider line
        m.setColor(BACKGROUND_GREY);
        m.drawRectangle(0, m.getSize().height(), m.getSize().width(), 35); // bottom bar
        
        m.setColor(APP_LABEL_TEXT_COLOR);
        m.setFont("Proxima Nova Reg.ttf", 10);
        
        // Input Channel Mode Selector
        m.setColor(200, 255);
        m.setFont("Proxima Nova Reg.ttf", 10);
        auto& inputLabel = m.draw<M1Label>(MurkaShape(m.getSize().width()/2 - 120 - 40, m.getSize().height() - 26, 60, 20));
        inputLabel.label = "INPUT";
        inputLabel.alignment = TEXT_CENTER;
        inputLabel.enabled = false;
        inputLabel.highlighted = false;
        inputLabel.commit();
        
        std::string inputLabelText = "";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeMono) inputLabelText = "MONO ";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeStereo) inputLabelText = "STEREO";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeLCR) inputLabelText = "LCR ";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeQuad) inputLabelText = "QUAD ";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeLCRS) inputLabelText = "LCRS ";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeAFormat) inputLabelText = "AFORMAT";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputMode5dot0) inputLabelText = "5.0 Film";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputMode5dot1Film) inputLabelText = "5.1 Film";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputMode5dot1DTS) inputLabelText = "5.1 DTS";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputMode5dot1SMTPE) inputLabelText = "5.1 SMPTE";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeBFOAACN) inputLabelText = "1st Order ACNSN3D";
        if (pannerState->m1Encode->getInputMode() == Mach1EncodeInputModeBFOAFUMA) inputLabelText = "1st Order FuMa";

        // INPUT DROPDOWN
        int dropdownItemHeight = 25;
        
        // STEREO hosts here
        if (processor->hostType.isAbletonLive() || processor->hostType.isReason() || processor->hostType.isLogic() || processor->hostType.isGarageBand() || processor->hostType.isFruityLoops() || processor->hostType.isBitwigStudio()) {
            // MONO & STEREO only
            auto& inputDropdownButton = m.draw<M1DropdownButton>({  m.getSize().width()/2 - 60 - 40,
                                                                    m.getSize().height() - 33,
                                                                    80, 30 })
                                                        .withLabel(inputLabelText).commit();
            std::vector<std::string> input_options = {"MONO", "STEREO"};
            auto& inputDropdownMenu = m.draw<M1DropdownMenu>({  m.getSize().width()/2 - 60 - 40,
                                                                m.getSize().height() - 33 - input_options.size() * dropdownItemHeight,
                                                                80, input_options.size() * dropdownItemHeight })
                                                        .controlling(&pannerState->inputType)
                                                        .withOptions(input_options);

            if (inputDropdownButton.pressed) {
                inputDropdownMenu.open();
            }

            inputDropdownMenu.optionHeight = dropdownItemHeight;
            inputDropdownMenu.fontSize = 9;
            inputDropdownMenu.commit();

            if (inputDropdownMenu.changed) {
                if (inputDropdownMenu.selectedOption == 0) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeMono);
                } else if (inputDropdownMenu.selectedOption == 1) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeStereo);
                }
                processor->parameterChanged(processor->paramInputMode, pannerState->m1Encode->getInputMode());
            }
        // PT or other hosts that support multichannel and need selector dropdown for >4 channel modes
        } else if (processor->hostType.isProTools() && processor->getMainBusNumInputChannels() >= 4) {
            // Displaying options only available as 4 channel INPUT
            // Dropdown is used for QUADMODE indication only
            auto& inputDropdownButton = m.draw<M1DropdownButton>({  m.getSize().width()/2 - 60 - 40,
                                                                    m.getSize().height() - 33,
                                                                    80, 30 })
                                                        .withLabel(inputLabelText).commit();
            std::vector<std::string> input_options = {"QUAD", "LCRS", "AFORMAT", "1OA-ACN", "1OA-FuMa"};
            auto& inputDropdownMenu = m.draw<M1DropdownMenu>({  m.getSize().width()/2 - 60 - 40,
                                                                m.getSize().height() - 33 - input_options.size() * dropdownItemHeight,
                                                                80, input_options.size() * dropdownItemHeight })
                                                        .controlling(&pannerState->inputType)
                                                        .withOptions(input_options);

            if (inputDropdownButton.pressed) {
                inputDropdownMenu.open();
            }

            inputDropdownMenu.optionHeight = dropdownItemHeight;
            inputDropdownMenu.fontSize = 9;
            inputDropdownMenu.commit();

            if (inputDropdownMenu.changed) {
                if (inputDropdownMenu.selectedOption == 0) {
                    pannerState->m1Encode->setInputMode(Mach1EncodeInputModeQuad);
                    pannerState->inputType = Mach1EncodeInputModeQuad;
                } else if (inputDropdownMenu.selectedOption == 1) {
                    pannerState->m1Encode->setInputMode(Mach1EncodeInputModeLCRS);
                    pannerState->inputType = Mach1EncodeInputModeLCRS;
                } else if (inputDropdownMenu.selectedOption == 2) {
                    pannerState->m1Encode->setInputMode(Mach1EncodeInputModeAFormat);
                    pannerState->inputType = Mach1EncodeInputModeAFormat;
                } else if (inputDropdownMenu.selectedOption == 3) {
                    pannerState->m1Encode->setInputMode(Mach1EncodeInputModeBFOAACN);
                    pannerState->inputType = Mach1EncodeInputModeBFOAACN;
                } else if (inputDropdownMenu.selectedOption == 4) {
                    pannerState->m1Encode->setInputMode(Mach1EncodeInputModeBFOAFUMA);
                    pannerState->inputType = Mach1EncodeInputModeBFOAFUMA;
                }
                processor->parameterChanged(processor->paramInputMode, pannerState->inputType);
            }
        // Multichannel DAWs
        } else {
            auto& inputDropdownButton = m.draw<M1DropdownButton>({  m.getSize().width()/2 - 60 - 40,
                                                                    m.getSize().height() - 33,
                                                                    80, 30 })
                                                        .withLabel(inputLabelText).commit();
            std::vector<std::string> input_options = {"MONO", "STEREO", "LCR ", "QUAD ", "LCRS ", "AFORMAT", "1OA-ACN", "1OA-FuMa", "5.0 Film", "5.1 Film", "5.1 DTS", "5.1 SMPTE"};
            auto& inputDropdownMenu = m.draw<M1DropdownMenu>({  m.getSize().width()/2 - 60 - 40,
                                                                m.getSize().height() - 33 - input_options.size() * dropdownItemHeight,
                                                                80, input_options.size() * dropdownItemHeight })
                                                        .controlling(&pannerState->inputType)
                                                        .withOptions(input_options);

            if (inputDropdownButton.pressed) {
                inputDropdownMenu.open();
            }

            inputDropdownMenu.optionHeight = dropdownItemHeight;
            inputDropdownMenu.fontSize = 9;
            inputDropdownMenu.commit();

            if (inputDropdownMenu.changed) {
                if (inputDropdownMenu.selectedOption == 0) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeMono);
                } else if (inputDropdownMenu.selectedOption == 1) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeStereo);
                } else if (inputDropdownMenu.selectedOption == 2) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeLCR);
                } else if (inputDropdownMenu.selectedOption == 3) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeQuad);
                } else if (inputDropdownMenu.selectedOption == 4) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeLCRS);
                } else if (inputDropdownMenu.selectedOption == 5) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeAFormat);
                } else if (inputDropdownMenu.selectedOption == 6) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeBFOAACN);
                } else if (inputDropdownMenu.selectedOption == 7) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputModeBFOAFUMA);
                } else if (inputDropdownMenu.selectedOption == 8) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputMode5dot0);
                } else if (inputDropdownMenu.selectedOption == 9) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputMode5dot1Film);
                } else if (inputDropdownMenu.selectedOption == 10) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputMode5dot1DTS);
                } else if (inputDropdownMenu.selectedOption == 11) {
                    processor->parameterChanged(processor->paramInputMode, pannerState->inputType = Mach1EncodeInputMode5dot1SMTPE);
                }
                processor->parameterChanged(processor->paramInputMode, pannerState->m1Encode->getInputMode());
            }
        }

        // OUTPUT DROPDOWN & LABELS
        if (!processor->hostType.isProTools() || (processor->hostType.isProTools() && processor->getMainBusNumInputChannels() >= 4)) {
            /// -> label
            m.setColor(200, 255);
            m.setFont("Proxima Nova Reg.ttf", 10);
            auto& arrowLabel = m.draw<M1Label>(MurkaShape(m.getSize().width()/2 - 20, m.getSize().height() - 26, 40, 20));
            arrowLabel.label = "-->";
            arrowLabel.alignment = TEXT_CENTER;
            arrowLabel.enabled = false;
            arrowLabel.highlighted = false;
            arrowLabel.commit();
            
            // OUTPUT DROPDOWN or LABEL
            m.setColor(200, 255);
            m.setFont("Proxima Nova Reg.ttf", 10);
            auto& outputLabel = m.draw<M1Label>(MurkaShape(m.getSize().width()/2 + 110, m.getSize().height() - 26, 60, 20));
            outputLabel.label = "OUTPUT";
            outputLabel.alignment = TEXT_CENTER;
            outputLabel.enabled = false;
            outputLabel.highlighted = false;
            outputLabel.commit();
            
            auto& outputDropdownButton = m.draw<M1DropdownButton>({ m.getSize().width()/2 + 20, m.getSize().height()-33,
                                                        80, 30 })
                                                        .withLabel(std::to_string(pannerState->m1Encode->getOutputChannelsCount())).commit();
            std::vector<std::string> output_options = {"M1Horizon-4", "M1Spatial-8", "M1Spatial-12", "M1Spatial-14", "M1Spatial-18", "M1Spatial-22", "M1Spatial-32", "M1Spatial-36", "M1Spatial-48", "M1Spatial-60"};
            auto& outputDropdownMenu = m.draw<M1DropdownMenu>({  m.getSize().width()/2 + 20,
                                                                m.getSize().height() - 33 - output_options.size() * dropdownItemHeight,
                                                                120, output_options.size() * dropdownItemHeight })
                                                        .controlling(&pannerState->inputType)
                                                        .withOptions(output_options);
            if (outputDropdownButton.pressed) {
                outputDropdownMenu.open();
            }
            
            outputDropdownMenu.optionHeight = dropdownItemHeight;
            outputDropdownMenu.fontSize = 9;
            outputDropdownMenu.commit();

            if (outputDropdownMenu.changed) {
                if (outputDropdownMenu.selectedOption == 0) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Horizon_4);
                } else if (outputDropdownMenu.selectedOption == 1) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_8);
                } else if (outputDropdownMenu.selectedOption == 2) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_12);
                } else if (outputDropdownMenu.selectedOption == 3) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_14);
                } else if (outputDropdownMenu.selectedOption == 4) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_18);
                } else if (outputDropdownMenu.selectedOption == 4) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_22);
                } else if (outputDropdownMenu.selectedOption == 4) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_32);
                } else if (outputDropdownMenu.selectedOption == 4) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_36);
                } else if (outputDropdownMenu.selectedOption == 4) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_48);
                } else if (outputDropdownMenu.selectedOption == 4) {
                    processor->parameterChanged(processor->paramOutputMode, pannerState->outputType = Mach1EncodeOutputModeM1Spatial_60);
                }
                processor->parameterChanged(processor->paramOutputMode, pannerState->m1Encode->getOutputMode());
            }
        } else {
            // PT & 4 channels
            // hide bottom bar
        }
    }
#endif // end of bottom bar macro check
    
    /// Panner label
    m.setColor(200, 255);
    m.setFont("Proxima Nova Reg.ttf", 10);
#ifdef CUSTOM_CHANNEL_LAYOUT
    auto& pannerLabel = m.draw<M1Label>(MurkaShape(m.getSize().width() - 100, m.getSize().height() - 30, 80, 20));
#else
    int labelYOffset;
    if (!processor->hostType.isProTools() || (processor->hostType.isProTools() && processor->getMainBusNumInputChannels() >= 4)) {
        labelYOffset = 26;
    } else {
        labelYOffset = 30;
    }
    auto& pannerLabel = m.draw<M1Label>(MurkaShape(m.getSize().width() - 100, m.getSize().height() - labelYOffset, 80, 20));
#endif
    pannerLabel.label = "PANNER";
    pannerLabel.alignment = TEXT_CENTER;
    pannerLabel.enabled = false;
    pannerLabel.highlighted = false;
    pannerLabel.commit();
    
    m.setColor(200, 255);
#ifdef CUSTOM_CHANNEL_LAYOUT
    m.drawImage(m1logo, 20, m.getSize().height() - 30, 161 / 3, 39 / 3);
#else
    if (!processor->hostType.isProTools() || (processor->hostType.isProTools() && processor->getMainBusNumInputChannels() >= 4)) {
        m.drawImage(m1logo, 20, m.getSize().height() - 26, 161 / 3, 39 / 3);
    } else {
        m.drawImage(m1logo, 20, m.getSize().height() - 30, 161 / 3, 39 / 3);
    }
#endif

	m.end();
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
