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
	pannerSettings = &processor->pannerSettings;
    mixerSettings = &processor->monitorSettings;
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
}

void PannerUIBaseComponent::render()
{
	currentMousePosition = m.currentContext.mousePosition * 0.7;
    
    float scale = (float)openGLContext.getRenderingScale() * 0.7; // (Desktop::getInstance().getMainMouseSource().getScreenPosition().x / 300.0); //  0.7;

    if (scale != m.getScreenScale()) {
        m.setScreenScale(scale);
        m.updateFontsTextures(&m);
        m.clearFontsTextures();
    }

   // This clears the context with a black background.
	//OpenGLHelpers::clear (Colours::black);

	m.setFont("Proxima Nova Reg.ttf", 10);
	m.begin();

	m.setColor(40, 40, 40, 255);
	m.clear();

	m.setColor(255);

	XYRD xyrd = { pannerSettings->x, pannerSettings->y, pannerSettings->azimuth, pannerSettings->diverge };
    auto & reticleField = m.drawWidget<PannerReticleField>(MurkaShape(25, 30, 400, 400));
    reticleField.controlling(&xyrd);
    
    reticleField.cursorHide = cursorHide;
    reticleField.cursorShow = cursorShow;
    reticleField.teleportCursor = teleportCursor;
    reticleField.shouldDrawDivergeGuideLine = divergeKnobDraggingNow;
    reticleField.shouldDrawRotateGuideLine = rotateKnobDraggingNow;
    reticleField.azimuth = pannerSettings->azimuth;
    reticleField.elevation = pannerSettings->elevation;
    reticleField.diverge = pannerSettings->diverge;
    reticleField.autoOrbit = pannerSettings->autoOrbit;
    reticleField.sRotate = pannerSettings->stereoOrbitAzimuth;
    reticleField.sSpread = pannerSettings->stereoSpread;
    reticleField.m1Encode = pannerSettings->m1Encode;
    reticleField.pannerState = pannerSettings;
    reticleField.monitorState = mixerSettings;
    reticleField.commit();
    
    if (reticleField.results) {
		convertXYtoRCRaw(pannerSettings->x, pannerSettings->y, pannerSettings->azimuth, pannerSettings->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerSettings->azimuth);
        processor->parameterChanged(processor->paramElevation, pannerSettings->diverge);
        processor->parameterChanged(processor->paramX, pannerSettings->x);
        processor->parameterChanged(processor->paramY, pannerSettings->y);
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
                                .controlling(&pannerSettings->x);
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
		convertXYtoRCRaw(pannerSettings->x, pannerSettings->y, pannerSettings->azimuth, pannerSettings->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerSettings->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerSettings->diverge);
        processor->parameterChanged(processor->paramX, pannerSettings->x);
        processor->parameterChanged(processor->paramY, pannerSettings->y);
	}
	m.setColor(200, 255);
    auto& xLabel = m.draw<M1Label>(MurkaShape(xOffset + 10 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    xLabel.label = "X";
    xLabel.alignment = TEXT_CENTER;
    xLabel.enabled = true;
    xLabel.highlighted = xKnob.hovered || reticleHoveredLastFrame;
    xLabel.commit();

	// Y
    auto& yKnob = m.draw<M1Knob>(MurkaShape(xOffset + 100, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerSettings->y);
    yKnob.rangeFrom = -100;
    yKnob.rangeTo = 100;
    yKnob.floatingPointPrecision = 1;
    yKnob.speed = knobSpeed;
    yKnob.defaultValue = 0;
    yKnob.isEndlessRotary = false;
    yKnob.enabled = true;
    yKnob.externalHover = reticleHoveredLastFrame;
    yKnob.cursorHide = cursorHide;
    yKnob.cursorShow = cursorShowAndTeleportBack;
    yKnob.commit();
    
    if (yKnob.changed) {
        convertXYtoRCRaw(pannerSettings->x, pannerSettings->y, pannerSettings->azimuth, pannerSettings->diverge);
        processor->parameterChanged(processor->paramAzimuth, pannerSettings->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerSettings->diverge);
        processor->parameterChanged(processor->paramX, pannerSettings->x);
        processor->parameterChanged(processor->paramY, pannerSettings->y);
    }
    
	m.setColor(200, 255);
    auto& yLabel = m.draw<M1Label>(MurkaShape(xOffset + 100 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    yLabel.label = "Y";
    yLabel.alignment = TEXT_CENTER;
    yLabel.enabled = true;
    yLabel.highlighted = yKnob.hovered || reticleHoveredLastFrame;
    yLabel.commit();

	// Rotation
    auto& rKnob = m.draw<M1Knob>(MurkaShape(xOffset + 190, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerSettings->azimuth);
    rKnob.rangeFrom = -180;
    rKnob.rangeTo = 180;
    rKnob.floatingPointPrecision = 1;
    rKnob.speed = knobSpeed;
    rKnob.defaultValue = 0;
    rKnob.isEndlessRotary = true;
    rKnob.enabled = true;
    rKnob.externalHover = reticleHoveredLastFrame;
    rKnob.cursorHide = cursorHide;
    rKnob.cursorShow = cursorShowAndTeleportBack;
    rKnob.commit();

    if (rKnob.changed) {
        convertRCtoXYRaw(pannerSettings->azimuth, pannerSettings->diverge, pannerSettings->x, pannerSettings->y);
        processor->parameterChanged(processor->paramAzimuth, pannerSettings->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerSettings->diverge);
        processor->parameterChanged(processor->paramX, pannerSettings->x);
        processor->parameterChanged(processor->paramY, pannerSettings->y);
    }
    
	rotateKnobDraggingNow = rKnob.draggingNow;
	m.setColor(200, 255);
    auto& rLabel = m.draw<M1Label>(MurkaShape(xOffset + 190 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    rLabel.label = "ROTATE";
    rLabel.alignment = TEXT_CENTER;
    rLabel.enabled = true;
    rLabel.highlighted = rKnob.hovered || reticleHoveredLastFrame;
    rLabel.commit();

	// Diverge
    auto& dKnob = m.draw<M1Knob>(MurkaShape(xOffset + 280, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerSettings->diverge);
    dKnob.rangeFrom = -100;
    dKnob.rangeTo = 100;
    dKnob.floatingPointPrecision = 1;
    dKnob.speed = knobSpeed;
    dKnob.defaultValue = 0;
    dKnob.isEndlessRotary = false;
    dKnob.enabled = true;
    dKnob.externalHover = reticleHoveredLastFrame;
    dKnob.cursorHide = cursorHide;
    dKnob.cursorShow = cursorShowAndTeleportBack;
    dKnob.commit();
    
    if (dKnob.changed) {
        processor->parameterChanged(processor->paramAzimuth, pannerSettings->azimuth);
        processor->parameterChanged(processor->paramDiverge, pannerSettings->diverge);
        processor->parameterChanged(processor->paramX, pannerSettings->x);
        processor->parameterChanged(processor->paramY, pannerSettings->y);
    }
    
	divergeKnobDraggingNow = dKnob.draggingNow;
	m.setColor(200, 255);
    auto& dLabel = m.draw<M1Label>(MurkaShape(xOffset + 280 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    dLabel.label = "DIVERGE";
    dLabel.alignment = TEXT_CENTER;
    dLabel.enabled = true;
    dLabel.highlighted = dKnob.hovered || reticleHoveredLastFrame;
    dLabel.commit();

	// Gain
    auto& gKnob = m.draw<M1Knob>(MurkaShape(xOffset + 370, yOffset, knobWidth, knobHeight))
                                .controlling(&pannerSettings->gain);
    gKnob.rangeFrom = -90;
    gKnob.rangeTo = 24;
    gKnob.postfix = "db";
    gKnob.prefix = std::string(pannerSettings->gain > 0 ? "+" : "");
    gKnob.floatingPointPrecision = 1;
    gKnob.speed = knobSpeed;
    gKnob.defaultValue = 0;
    gKnob.isEndlessRotary = false;
    gKnob.enabled = true;
    gKnob.externalHover = false;
    gKnob.cursorHide = cursorHide;
    gKnob.cursorShow = cursorShowAndTeleportBack;
    gKnob.commit();
    
    if (gKnob.changed) {
        processor->parameterChanged("gain", pannerSettings->gain);
    }

	m.setColor(200, 255);
    auto& gLabel = m.draw<M1Label>(MurkaShape(xOffset + 370 + M1LabelOffsetX, yOffset - M1LabelOffsetY, knobWidth, knobHeight));
    gLabel.label = "GAIN";
    gLabel.alignment = TEXT_CENTER;
    gLabel.enabled = true;
    gLabel.highlighted = gKnob.hovered || reticleHoveredLastFrame;
    gLabel.commit();

	// S Rotation 
    auto& srKnob = m.draw<M1Knob>(MurkaShape(xOffset + 190,
                                             yOffset + 150, knobWidth, knobHeight))
                                    .controlling(&pannerSettings->stereoOrbitAzimuth);
    srKnob.rangeFrom = -180;
    srKnob.rangeTo = 180;
    srKnob.prefix = "º";
    srKnob.postfix = "";
    srKnob.floatingPointPrecision = 1;
    srKnob.speed = knobSpeed;
    srKnob.defaultValue = 0;
    srKnob.isEndlessRotary = true;
    srKnob.enabled = !pannerSettings->autoOrbit;
    srKnob.externalHover = false;
    srKnob.cursorHide = cursorHide;
    srKnob.cursorShow = cursorShowAndTeleportBack;
    srKnob.commit();
    
    if (srKnob.changed) {
        processor->parameterChanged("orbitAzimuth", pannerSettings->stereoOrbitAzimuth);
    }

	m.setColor(200, 255);
    auto& srLabel = m.draw<M1Label>(MurkaShape(xOffset + 190 + M1LabelOffsetX,
                                              yOffset - M1LabelOffsetY + 150, knobWidth, knobHeight));
    srLabel.label = "S ROTATE";
    srLabel.alignment = TEXT_CENTER;
    srLabel.enabled = !pannerSettings->autoOrbit;
    srLabel.highlighted = srKnob.hovered || reticleHoveredLastFrame;
    srLabel.commit();

	// S Spread

	// TODO didChangeOutsideThisThread ???
    auto& ssKnob = m.draw<M1Knob>(MurkaShape(xOffset + 280,
                                             yOffset + 150, knobWidth, knobHeight))
                                    .controlling(&pannerSettings->stereoOrbitAzimuth);
    ssKnob.rangeFrom = 0;
    ssKnob.rangeTo = 100;
    ssKnob.prefix = "";
    ssKnob.postfix = "";
    ssKnob.floatingPointPrecision = 1;
    ssKnob.speed = knobSpeed;
    ssKnob.defaultValue = 50;
    ssKnob.isEndlessRotary = false;
    ssKnob.enabled = (pannerSettings->inputType > 1) ? true : false;
    ssKnob.externalHover = false;
    ssKnob.cursorHide = cursorHide;
    ssKnob.cursorShow = cursorShowAndTeleportBack;
    ssKnob.commit();
    
    if (ssKnob.changed) {
        processor->parameterChanged("orbitSpread", pannerSettings->stereoSpread);
    }

	//M1LabelAnimation = A((getLatestDrawnWidget<M1Knob>(m.latestContext)->hovered || reticleHoveredLastFrame) && (pannerSettings->inputType > 1) ? true : false);
	m.setColor(200, 255);
    auto& ssLabel = m.draw<M1Label>(MurkaShape(xOffset + 280 - 2 + M1LabelOffsetX,
                                               yOffset - M1LabelOffsetY + 150,
                                               knobWidth + 10, knobHeight));
    ssLabel.label = "S SPREAD";
    ssLabel.alignment = TEXT_CENTER;
    ssLabel.enabled = (pannerSettings->inputType > 1) ? true : false;
    ssLabel.highlighted = ssKnob.hovered || reticleHoveredLastFrame;
    ssLabel.commit();

	// S Pan
    auto& spKnob = m.draw<M1Knob>(MurkaShape(xOffset + 370,
                                            yOffset + 150, knobWidth, knobHeight))
                                            .controlling(&pannerSettings->stereoInputBalance);
    spKnob.rangeFrom = -1;
    spKnob.rangeTo = 1;
    spKnob.prefix = "";
    spKnob.postfix = "";
    spKnob.floatingPointPrecision = 1;
    spKnob.speed = knobSpeed;
    spKnob.defaultValue = 0;
    spKnob.isEndlessRotary = false;
    spKnob.enabled = (pannerSettings->inputType > 1) ? true : false;
    spKnob.externalHover = false;
    spKnob.cursorHide = cursorHide;
    spKnob.cursorShow = cursorShowAndTeleportBack;
    spKnob.commit();
    
//    m.prepare<Label>({100, 100, 500, 100}).text("Label").draw();
    
    if (spKnob.changed) {
        processor->parameterChanged("orbitBalance", pannerSettings->stereoInputBalance);
    }

	m.setColor(200, 255);
    auto& spLabel = m.draw<M1Label>(MurkaShape(xOffset + 370 + M1LabelOffsetX,
                                               yOffset - M1LabelOffsetY + 150, knobWidth, knobHeight));
    spLabel.label = "S PAN";
    spLabel.alignment = TEXT_CENTER;
    spLabel.enabled = (pannerSettings->inputType > 1) ? true : false;
    spLabel.highlighted = spKnob.hovered || reticleHoveredLastFrame;
    spLabel.commit();

	// Z
    auto& zKnob = m.draw<M1Knob>(MurkaShape(xOffset + 450, yOffset, knobWidth, knobHeight))
                                            .controlling(&pannerSettings->elevation);
    zKnob.rangeFrom = -90;
    zKnob.rangeTo = 90;
    zKnob.prefix = "";
    zKnob.postfix = "";
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
        processor->parameterChanged("elevation", pannerSettings->elevation);
    }

	bool zHovered = zKnob.hovered;

	m.setColor(200, 255);
    auto& zLabel = m.draw<M1Label>(MurkaShape(xOffset + 450 + M1LabelOffsetX, yOffset - M1LabelOffsetY,
                                              knobWidth, knobHeight));
    zLabel.label = "Z";
    zLabel.alignment = TEXT_CENTER;
    zLabel.enabled = true;
    zLabel.highlighted = zKnob.hovered || reticleHoveredLastFrame;
    zLabel.commit();

	// CHECKBOXES
	float checkboxSlotHeight = 28;
    
    auto& overlayCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 0,
                                                200, 20 })
                                                .controlling(&pannerSettings->overlay)
                                                .withLabel("OVERLAY");
    overlayCheckbox.enabled = true;
    overlayCheckbox.commit();
    
    if (overlayCheckbox.changed) {
        setOverlayVisible(pannerSettings->overlay);
    }
        
    auto& isotropicCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 1,
                                                200, 20 })
                                                .controlling(&pannerSettings->isotropicMode)
                                                .withLabel("ISOTROPIC");
    isotropicCheckbox.enabled = true;
    isotropicCheckbox.commit();

    auto& equalPowerCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 2,
                                                200, 20 })
                                                .controlling(&pannerSettings->equalpowerMode)
                                                .withLabel("EQUALPOWER");
    equalPowerCheckbox.enabled = true;
    equalPowerCheckbox.commit();

    if (isotropicCheckbox.changed || equalPowerCheckbox.changed) {
        // update pannerSettings storage of current states
        if (isotropicCheckbox.checked) {
            if (equalPowerCheckbox.checked) {
                processor->parameterChanged("isotropicEncodeMode", pannerSettings->pannerMode = Mach1EncodePannerModeIsotropicEqualPower);
            } else {
                processor->parameterChanged("isotropicEncodeMode", pannerSettings->pannerMode = Mach1EncodePannerModeIsotropicLinear);
            }
        } else {
            processor->parameterChanged("isotropicEncodeMode", pannerSettings->pannerMode = Mach1EncodePannerModePeriphonicLinear);
        }
    }

    auto& autoOrbitCheckbox = m.draw<M1Checkbox>({ 557, 475 + checkboxSlotHeight * 3,
                                                200, 20 })
                                                .controlling(&pannerSettings->autoOrbit)
                                                .withLabel("AUTO ORBIT");
    autoOrbitCheckbox.enabled = true;
    autoOrbitCheckbox.commit();
    
    if (autoOrbitCheckbox.changed) {
        processor->parameterChanged("autoOrbit", pannerSettings->autoOrbit);
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
    pitchWheel.mixerPitch = mixerSettings->pitch;
    pitchWheel.dataToControl = &pannerSettings->elevation;
    pitchWheel.commit();
    
    if (pitchWheel.changed) {
        processor->parameterChanged("elevation", pannerSettings->elevation);
    }
    
//	if (m.drawWidget<M1PitchWheel>(m, &pannerSettings->elevation, { cursorHide, cursorShow, 10., 90., -90., zHovered, mixerState.pitch }, { 445, 30 - 10, 80, 400 + 20 })) {
//		// update this parameter here, notifying host
//		processor->updateCustomParameter(&pannerSettings->elevation);
//	}
	pitchWheelHoveredAtLastFrame = pitchWheel.hovered;

	// Drawing volume meters
	std::vector<float> volumesInDb{ -10, -10 };
	if (volumesInDb.size() > 0) {
		std::vector<float> volumes = volumesInDb;

		if (processor->m1Encode.getOutputChannelsCount() > 0) {
            m.setFont("Proxima Nova Reg.ttf", 7);
			for (int channelIndex = 0; channelIndex < processor->m1Encode.getOutputChannelsCount(); channelIndex++) {
                auto& volumeDisplayLine = m.draw<M1VolumeDisplayLine>({ 560 + 15 * channelIndex, 30, 10, 400 }).withVolume(volumes[channelIndex]).withCoeff(processor->outputMeterValuedB[channelIndex]).commit();

                m.setColor(210, 255);
                m.draw<M1Label>({ 560 + 15 * channelIndex, 433, 60, 50 }).text(std::to_string(channelIndex + 1)).commit();

				//                    if ((pannerSettings->inputType > 1) ? true : false) {
				//                        //drawWidget<M1Label>(m, { ofToString((int)volumes[1]) }, { 580, 10, 30, 30 });
				//                        drawWidget<M1VolumeDisplayLine>(m, {volumes[1]}, {580, 30, 10, 400});
				//                        drawWidget<M1Label>(m, {"2"}, {580, 433, 60, 50});
				//                    }
			}

			m.setColor(210, 255);
			for (int i = 0; i <= 56; i += 6) {
				//                    float db = ofMap(i, 0, 100, 0, -144); // 144 from M1VolumeDisplayLine math
				float db = -i + 12;

				float y = i * 7;
                m.draw<M1Label>({ 520 + 40 + 15 * processor->m1Encode.getOutputChannelsCount(), 30 + y - m.getCurrentFont()->getLineHeight() / 2,
                    30, 30 }).text( i != 100 ? std::to_string((int)db) : "dB" ).commit();
				//m.drawLine(600, 30 + y, 600 + 20, 30 + y);
			}
		}
	}

    m.setColor(200, 255);
    m.setFont("Proxima Nova Reg.ttf", 10);
    auto& pannerLabel = m.draw<M1Label>(MurkaShape(m.getSize().width() - 100, m.getSize().height() - 30, 80, 20));
    pannerLabel.label = "PANNER";
    pannerLabel.alignment = TEXT_CENTER;
    pannerLabel.enabled = false;
    pannerLabel.highlighted = false;
    pannerLabel.commit();
    
    m1logo.loadFromRawData(BinaryData::mach1logo_png, BinaryData::mach1logo_pngSize);
    m.drawImage(m1logo, 20, m.getSize().height() - 30, 161 / 3, 39 / 3);
    
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
