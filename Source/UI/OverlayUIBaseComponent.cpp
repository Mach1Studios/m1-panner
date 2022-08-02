#include "OverlayUIBaseComponent.h"
#include "OverlayReticleField.h"

using namespace murka;

//==============================================================================
OverlayUIBaseComponent::OverlayUIBaseComponent(M1PannerAudioProcessor* processor_)
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


void OverlayUIBaseComponent::convertRCtoXYRaw(float r, float d, float &x, float &y) {
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

void OverlayUIBaseComponent::convertXYtoRCRaw(float x, float y, float &r, float &d) {
	if (x == 0 && y == 0) {
		r = 0;
		d = 0;
	}
	else {
		d = sqrtf(x*x + y * y) / sqrt(2.0);

		float rotation_radian = atan2(x, y);//acos(x/d);
		r = juce::radiansToDegrees(rotation_radian);
	}
}

OverlayUIBaseComponent::~OverlayUIBaseComponent()
{
}

//==============================================================================
void OverlayUIBaseComponent::initialise()
{
	JuceMurkaBaseComponent::initialise();

    //TODO: move this to be static loaded?
    if ((juce::SystemStats::getOperatingSystemType() & juce::SystemStats::MacOSX) != 0) {
        std::string resourcesPath = juce::File::getSpecialLocation(juce::File::SpecialLocationType::userApplicationDataDirectory).getChildFile("Application Support/Mach1 Spatial System/resources").getFullPathName().toStdString();
    } else {
        std::string resourcesPath = juce::File::getSpecialLocation(juce::File::SpecialLocationType::userApplicationDataDirectory).getChildFile("Mach1 Spatial System/resources").getFullPathName().toStdString();
    }
	
	makeTransparent();
}

void OverlayUIBaseComponent::render()
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

	// TODO
	m.setFont("Proxima Nova Reg.ttf", 10);
	m.begin();

	m.setColor(0, 0);
	m.clear();

	m.pushView();
	m.pushStyle();

	double time = m.getElapsedTime();

	m.setColor(40, 40, 40);


	float knobSpeed = 250; // TODO: if shift pressed, lower speed

	if (pannerState) {
		XYRZ xyrz = { pannerState->x, pannerState->y, pannerState->azimuth, pannerState->elevation };
        auto& overlayReticleField = m.draw<OverlayReticleField>({0, 0, getWidth() / m.getScreenScale(), getHeight() / m.getScreenScale()}).controlling(&xyrz);
        overlayReticleField.cursorHide = cursorHide;
        overlayReticleField.cursorShow = cursorShow;
        overlayReticleField.teleportCursor = teleportCursor;
        overlayReticleField.shouldDrawDivergeLine = divergeKnobDraggingNow;
        overlayReticleField.shouldDrawRotateLine = rotateKnobDraggingNow;
        overlayReticleField.m1Encode = pannerState->m1Encode;
        overlayReticleField.sRotate = pannerState->stereoOrbitAzimuth;
        overlayReticleField.sSpread = pannerState->stereoSpread;
		overlayReticleField.commit();

        if (overlayReticleField.changed) {
			convertRCtoXYRaw(pannerState->azimuth, pannerState->diverge, pannerState->x, pannerState->y);
            processor->parameterChanged(processor->paramAzimuth, pannerState->azimuth);
            processor->parameterChanged(processor->paramElevation, pannerState->elevation);
			//?
		}
		reticleHoveredLastFrame = overlayReticleField.reticleHoveredLastFrame;

	}

	int xOffset = 20;
	int yOffset = 0;
	int knobWidth = 70;
	int knobHeight = 87;
	int labelOffsetY = 30;

	m.setCircleResolution(128);

	float labelAnimation = 0; // we will get the hover from knobs to highlight labels
	m.setFont("Proxima Nova Reg.ttf", 10);

	// Diverge
	if (pannerState) {
        
        auto& divergeKnob = m.draw<M1Knob>({xOffset, m.getWindowHeight() - knobHeight - 20, knobWidth, knobHeight})
            .controlling(&pannerState->diverge);
        divergeKnob.rangeFrom = -100;
        divergeKnob.rangeTo = 100;
        divergeKnob.floatingPointPrecision = 1;
        divergeKnob.speed = knobSpeed;
        divergeKnob.externalHover = reticleHoveredLastFrame;
        divergeKnob.cursorHide = cursorHide;
        divergeKnob.cursorShow = cursorShow;
		divergeKnob.commit();

        if (divergeKnob.changed) {
            // update this parameter here, notifying host
            convertRCtoXYRaw(pannerState->azimuth, pannerState->diverge, pannerState->x, pannerState->y);
            processor->parameterChanged(processor->paramDiverge, pannerState->diverge);
            //?
        }
        
		labelAnimation = m.A(divergeKnob.hovered || reticleHoveredLastFrame);
		divergeKnobDraggingNow = divergeKnob.draggingNow;
		m.setColor(210 + 40 * labelAnimation, 255);
        auto & dLabel = m.draw<M1Label>({xOffset, m.getWindowHeight() - knobHeight - 20 - labelOffsetY, knobWidth, knobHeight}).text("DIVERGE");
        dLabel.alignment = TEXT_CENTER;
	}

	m.popStyle();
	m.popView();

	m.end();
} 

//==============================================================================
void OverlayUIBaseComponent::paint (juce::Graphics& g)
{
    // You can add your component specific drawing code here!
    // This will draw over the top of the openGL background.
}

void OverlayUIBaseComponent::resized()
{
    // This is called when the OverlayUIBaseComponent is resized.
    // If you add any child components, this is where you should
    // update their positions.
}
