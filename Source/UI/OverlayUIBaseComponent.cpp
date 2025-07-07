#include "OverlayUIBaseComponent.h"
#include "OverlayReticleField.h"

using namespace murka;

//==============================================================================
OverlayUIBaseComponent::OverlayUIBaseComponent(M1PannerAudioProcessor* processor_)
{
    // Make sure you set the size of the component after
    // you add any child components.
    setSize(getWidth(), getHeight());

    processor = processor_;
    pannerState = &processor->pannerSettings;
    monitorState = &processor->monitorSettings;
}

struct Line2D
{
    Line2D(double x, double y, double x2, double y2) : x{ x }, y{ y }, x2{ x2 }, y2{ y2 } {};

    MurkaPoint p() const
    {
        return { x, y };
    }

    MurkaPoint v() const
    {
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
inline double intersection(const Line2D& a, const Line2D& b)
{
    const double Precision = std::sqrt(std::numeric_limits<double>::epsilon());
    double d = a.v().x * b.v().y - a.v().y * b.v().x;
    if (std::abs(d) < Precision)
        return std::numeric_limits<double>::quiet_NaN();
    else
    {
        double n = (b.p().x - a.p().x) * b.v().y
                   - (b.p().y - a.p().y) * b.v().x;
        return n / d;
    }
}

/// The intersection of two lines.
/// \NOTE The result is a Point2D having the coordinates
///       std::numeric_limits<double>::quiet_NaN() if the lines do not
///       intersect.
inline MurkaPoint intersection_point(const Line2D& a, const Line2D& b)
{
    // Line2D has an operator () (double r) returning p() + r * v()
    return a.p() + a.v() * (intersection(a, b));
}

void OverlayUIBaseComponent::convertRCtoXYRaw(float r, float d, float& x, float& y)
{
    x = cos(juce::degreesToRadians(-r + 90)) * d * sqrt(2);
    y = sin(juce::degreesToRadians(-r + 90)) * d * sqrt(2);
    if (x > 100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { 100, -100, 100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (y > 100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { -100, 100, 100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (x < -100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { -100, -100, -100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (y < -100)
    {
        auto intersection = intersection_point({ 0, 0, x, y },
            { -100, -100, 100, -100 });
        x = intersection.x;
        y = intersection.y;
    }
}

void OverlayUIBaseComponent::convertXYtoRCRaw(float x, float y, float& r, float& d)
{
    if (x == 0 && y == 0)
    {
        r = 0;
        d = 0;
    }
    else
    {
        d = sqrtf(x * x + y * y) / sqrt(2.0);

        float rotation_radian = atan2(x, y); //acos(x/d);
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
    makeTransparent();
}

void OverlayUIBaseComponent::draw()
{
    // TODO: Remove this and rescale all sizing and positions
    float scale = (float)openGLContext.getRenderingScale() * 0.7;
    if (scale != m.getScreenScale())
    {
        m.setScreenScale(scale);
        m.reloadFonts(&m);
    }

    // Storing mouse for the curorHide() and cursorShow() functions
    currentMousePosition = getLocalPoint(nullptr, Desktop::getMousePosition());

    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);
    m.setColor(0, 0);
    m.clear();
    m.pushView();
    m.pushStyle();

    m.setColor(BACKGROUND_GREY);

    // Read atomic values once for consistency during UI rendering
    float currentX = pannerState->x.load();
    float currentY = pannerState->y.load();
    float currentAzimuth = pannerState->azimuth.load();
    float currentDiverge = pannerState->diverge.load();
    float currentElevation = pannerState->elevation.load();
    float currentGain = pannerState->gain.load();
    float currentStereoOrbitAzimuth = pannerState->stereoOrbitAzimuth.load();
    float currentStereoSpread = pannerState->stereoSpread.load();
    float currentStereoInputBalance = pannerState->stereoInputBalance.load();
    bool currentOverlay = pannerState->overlay.load();
    bool currentIsotropicMode = pannerState->isotropicMode.load();
    bool currentEqualpowerMode = pannerState->equalpowerMode.load();
    bool currentGainCompensationMode = pannerState->gainCompensationMode.load();
    bool currentAutoOrbit = pannerState->autoOrbit.load();
    bool currentLockOutputLayout = pannerState->lockOutputLayout.load();

    float knobSpeed = 250; // TODO: if shift pressed, lower speed

    if (pannerState)
    {
        // Read atomic values once for consistency
        float currentX = pannerState->x.load();
        float currentY = pannerState->y.load();
        float currentAzimuth = pannerState->azimuth.load();
        float currentElevation = pannerState->elevation.load();
        float currentDiverge = pannerState->diverge.load();
        float currentStereoOrbitAzimuth = pannerState->stereoOrbitAzimuth.load();
        float currentStereoSpread = pannerState->stereoSpread.load();

        XYRZ xyrz = { currentX, currentY, currentAzimuth, currentElevation };
        auto& overlayReticleField = m.prepare<OverlayReticleField>({ 0, 0, m.getSize().width(), m.getSize().height() }).controlling(&xyrz);
        overlayReticleField.cursorHide = cursorHide;
        overlayReticleField.cursorShow = cursorShow;
        overlayReticleField.teleportCursor = teleportCursor;
        overlayReticleField.shouldDrawDivergeLine = divergeKnobDraggingNow;
        overlayReticleField.shouldDrawRotateLine = rotateKnobDraggingNow;
        overlayReticleField.m1Encode = &pannerState->m1Encode;
        overlayReticleField.sRotate = currentStereoOrbitAzimuth;
        overlayReticleField.sSpread = currentStereoSpread;
        overlayReticleField.isConnected = processor->pannerOSC->isConnected();
        overlayReticleField.track_color = processor->osc_colour;
        overlayReticleField.monitorState = monitorState;
        overlayReticleField.pannerState = pannerState;
        overlayReticleField.draw();

        if (overlayReticleField.changed)
        {
            // Update atomic values from UI changes
            pannerState->x.store(std::get<0>(xyrz));
            pannerState->y.store(std::get<1>(xyrz));
            pannerState->azimuth.store(std::get<2>(xyrz));
            pannerState->elevation.store(std::get<3>(xyrz));

            float newX, newY;
            convertRCtoXYRaw(std::get<2>(xyrz), currentDiverge, newX, newY);
            pannerState->x.store(newX);
            pannerState->y.store(newY);

            auto& params = processor->getValueTreeState();
            auto* paramAzimuth = params.getParameter(processor->paramAzimuth);
            paramAzimuth->setValueNotifyingHost(paramAzimuth->convertTo0to1(std::get<2>(xyrz)));
            auto* paramElevation = params.getParameter(processor->paramElevation);
            paramElevation->setValueNotifyingHost(paramElevation->convertTo0to1(std::get<3>(xyrz)));
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
    m.setFontFromRawData(PLUGIN_FONT, BINARYDATA_FONT, BINARYDATA_FONT_SIZE, DEFAULT_FONT_SIZE);

    // Diverge
    if (pannerState)
    {
        float localDiverge = currentDiverge;
        auto& divergeKnob = m.prepare<M1Knob>({ xOffset, m.getWindowHeight() - knobHeight - 20, knobWidth, knobHeight })
                                .controlling(&localDiverge);
        divergeKnob.rangeFrom = -100;
        divergeKnob.rangeTo = 100;
        divergeKnob.floatingPointPrecision = 1;
        divergeKnob.speed = knobSpeed;
        divergeKnob.externalHover = reticleHoveredLastFrame;
        divergeKnob.cursorHide = cursorHide;
        divergeKnob.cursorShow = cursorShowAndTeleportBack;
        divergeKnob.draw();

        if (divergeKnob.changed)
        {
            // update this parameter here, notifying host
            pannerState->diverge.store(localDiverge);
            float newX, newY;
            convertRCtoXYRaw(pannerState->azimuth.load(), localDiverge, newX, newY);
            pannerState->x.store(newX);
            pannerState->y.store(newY);
            auto& params = processor->getValueTreeState();
            auto* param = params.getParameter(processor->paramDiverge);
            param->setValueNotifyingHost(param->convertTo0to1(localDiverge));
        }

        labelAnimation = m.A(divergeKnob.hovered || reticleHoveredLastFrame);
        divergeKnobDraggingNow = divergeKnob.draggingNow;
        m.setColor(210 + 40 * labelAnimation, 255);
        auto& dLabel = m.prepare<M1Label>({ xOffset, m.getWindowHeight() - knobHeight - 20 - labelOffsetY, knobWidth, knobHeight }).text("DIVERGE");
        dLabel.alignment = TEXT_CENTER;
    }

    // Draw a border around the window
    m.setColor(255, 255, 255, 100); // Semi-transparent white
    m.setLineWidth(2);
    m.disableFill();
    m.drawRectangle(0, 0, m.getSize().width(), m.getSize().height());

    // Draw corner markers for easier resizing
    int cornerSize = 10;
    m.setColor(255, 255, 255, 100); // More visible white for corners
    m.setLineWidth(2);

    // Bottom-left corner
    m.drawLine(0, m.getSize().height() - cornerSize, 0, m.getSize().height());
    m.drawLine(0, m.getSize().height(), cornerSize, m.getSize().height());

    // Bottom-right corner
    m.drawLine(m.getSize().width() - cornerSize, m.getSize().height(), m.getSize().width(), m.getSize().height());
    m.drawLine(m.getSize().width(), m.getSize().height() - cornerSize, m.getSize().width(), m.getSize().height());

    m.popStyle();
    m.popView();

    m.end();
}

//==============================================================================
void OverlayUIBaseComponent::paint(juce::Graphics& g)
{
    // This will draw over the top of the openGL background.
}

void OverlayUIBaseComponent::resized()
{
    // This is called when the OverlayUIBaseComponent is resized.
}
