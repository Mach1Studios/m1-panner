#include "NativePannerReticleComponent.h"

namespace {

juce::Colour toColour(const juce::OSCColour& colour)
{
    return juce::Colour(colour.red, colour.green, colour.blue, colour.alpha);
}

juce::Colour rgb(unsigned char r, unsigned char g, unsigned char b)
{
    return juce::Colour::fromRGB(r, g, b);
}

juce::Colour rgba(unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    return juce::Colour::fromRGBA(r, g, b, a);
}

} // namespace

NativePannerReticleComponent::NativePannerReticleComponent(M1PannerAudioProcessor& processorToUse)
    : processor(processorToUse), pannerState(processorToUse.pannerSettings), monitorState(processorToUse.monitorSettings)
{
    startTimerHz(30);
}

NativePannerReticleComponent::~NativePannerReticleComponent()
{
    endGesture();
}

void NativePannerReticleComponent::timerCallback()
{
    repaint();
}

void NativePannerReticleComponent::setGuideLinesVisible(bool divergeVisible, bool rotateVisible)
{
    drawDivergeGuideLine = divergeVisible;
    drawRotateGuideLine = rotateVisible;
    repaint();
}

bool NativePannerReticleComponent::isReticleHoveredOrDragging() const
{
    return hovered || dragging;
}

juce::Point<float> NativePannerReticleComponent::reticlePositionForBounds(juce::Rectangle<float> bounds) const
{
    return {
        bounds.getCentreX() + (pannerState.x / 100.0f) * bounds.getWidth() * 0.5f,
        bounds.getCentreY() + (-pannerState.y / 100.0f) * bounds.getHeight() * 0.5f
    };
}

void NativePannerReticleComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto center = bounds.getCentre();
    const auto reticlePosition = reticlePositionForBounds(bounds);
    const bool reticleHovered = dragging || reticlePosition.getDistanceFrom(getMouseXYRelative().toFloat()) <= 12.0f;

    g.fillAll(rgb(BACKGROUND_GREY));

    g.setColour(rgba(GRID_LINES_1_RGBA));
    const auto smallStep = bounds.getWidth() / 96.0f;
    if (monitorState.monitor_mode >= 0 && monitorState.monitor_mode < 3 && monitorState.monitor_mode != 1) {
        for (int i = 0; i < 96; ++i) {
            const auto position = smallStep * static_cast<float>(i);
            g.drawVerticalLine(juce::roundToInt(position), 0.0f, bounds.getHeight());
            g.drawHorizontalLine(juce::roundToInt(position), 0.0f, bounds.getWidth());
        }
    }

    g.setColour(rgb(GRID_LINES_2));
    const auto largeStep = bounds.getWidth() / 4.0f;
    for (int i = 1; i < 4; ++i) {
        const auto position = largeStep * static_cast<float>(i);
        g.drawVerticalLine(juce::roundToInt(position), 0.0f, bounds.getHeight());
        g.drawHorizontalLine(juce::roundToInt(position), 0.0f, bounds.getWidth());
    }

    g.setColour(rgba(GRID_LINES_3_RGBA));
    g.drawLine(bounds.getTopLeft().x, bounds.getTopLeft().y, bounds.getBottomRight().x, bounds.getBottomRight().y, 1.0f);
    g.drawLine(bounds.getRight(), bounds.getY(), bounds.getX(), bounds.getBottom(), 1.0f);

    if (monitorState.monitor_mode >= 0 && monitorState.monitor_mode < 3 && monitorState.monitor_mode != 1) {
        g.setColour(rgba(GRID_LINES_3_RGBA));
        g.drawEllipse(center.x - bounds.getHeight() * 0.25f,
                      center.y - bounds.getHeight() * 0.25f,
                      bounds.getHeight() * 0.5f,
                      bounds.getHeight() * 0.5f,
                      1.5f);
        g.drawEllipse(bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight(), 1.5f);
    }

    if (processor.pannerOSC->isConnected() && monitorState.monitor_mode >= 0 && monitorState.monitor_mode < 3 && monitorState.monitor_mode != 1) {
        drawMonitorYaw(g, bounds);
    }

    if (monitorState.monitor_mode == 1) {
        auto footerBounds = bounds;
        g.setColour(rgb(BACKGROUND_GREY));
        g.fillRect(footerBounds.removeFromBottom(16.0f));
        g.setColour(rgb(ENABLED_PARAM));
        g.setFont(juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 4)));
        g.drawText("STEREO SAFE MODE", getLocalBounds().removeFromBottom(18), juce::Justification::centred, false);
    }

    if (dragging || drawRotateGuideLine) {
        g.setColour(rgb(M1_ACTION_YELLOW));
        const auto radius = (pannerState.diverge / 100.0f) * bounds.getWidth() / std::sqrt(2.0f);
        g.drawEllipse(center.x - radius, center.y - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }

    if (dragging || drawDivergeGuideLine) {
        g.setColour(rgb(M1_ACTION_YELLOW));
        const auto delta = reticlePosition - center;
        g.drawLine(center.x + delta.x * 30.0f,
                   center.y + delta.y * 30.0f,
                   center.x - delta.x * 30.0f,
                   center.y - delta.y * 30.0f,
                   1.5f);
    }

    drawAdditionalReticles(g, bounds, reticleHovered);

    const auto elevationRadius = 2.0f * (pannerState.elevation / 90.0f);
    g.setColour(rgb(M1_ACTION_YELLOW));
    g.fillEllipse(juce::Rectangle<float>(20.0f + 3.0f * static_cast<float>(reticleHovered) + elevationRadius,
                                         20.0f + 3.0f * static_cast<float>(reticleHovered) + elevationRadius)
                      .withCentre(reticlePosition));
    g.setColour(rgb(BACKGROUND_GREY));
    g.fillEllipse(juce::Rectangle<float>(16.0f + 3.0f * static_cast<float>(reticleHovered) + elevationRadius,
                                         16.0f + 3.0f * static_cast<float>(reticleHovered) + elevationRadius)
                      .withCentre(reticlePosition));
    g.setColour(rgb(M1_ACTION_YELLOW));
    g.fillEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre(reticlePosition));

    hovered = reticleHovered;
}

void NativePannerReticleComponent::drawMonitorYaw(juce::Graphics& g, juce::Rectangle<float> bounds) const
{
    const auto center = bounds.getCentre();
    const auto yawRadians = juce::degreesToRadians(monitorState.yaw - 90.0f);
    const auto diameter = bounds.getHeight() * 0.5f;
    const auto end = juce::Point<float>(std::cos(yawRadians) * diameter + center.x,
                                        std::sin(yawRadians) * diameter + center.y);

    g.setColour(rgb(ENABLED_PARAM));
    g.drawLine(center.x, center.y, end.x, end.y, 1.5f);
}

void NativePannerReticleComponent::drawAdditionalReticles(juce::Graphics& g, juce::Rectangle<float> bounds, bool reticleHovered) const
{
    const auto points = pannerState.m1Encode.getPoints();
    const auto pointNames = pannerState.m1Encode.getPointsNames();
    const auto inputMode = pannerState.m1Encode.getInputMode();

    if (pannerState.m1Encode.getInputChannelsCount() <= 1) {
        return;
    }

    for (int i = 0; i < pannerState.m1Encode.getPointsCount(); ++i) {
        auto point = juce::Point<float>((points[i].z + 1.0f) * bounds.getWidth() * 0.5f,
                                        (-points[i].x + 1.0f) * bounds.getHeight() * 0.5f);
        point.x = juce::jlimit(bounds.getX(), bounds.getRight(), point.x);
        point.y = juce::jlimit(bounds.getY(), bounds.getBottom(), point.y);

        float sizeMultiplier = 1.5f;
        if (inputMode == Mach1EncodeInputMode::Stereo || inputMode == Mach1EncodeInputMode::LCR) {
            sizeMultiplier = 1.0f;
        } else if (inputMode == Mach1EncodeInputMode::AFormat) {
            sizeMultiplier = 2.0f;
        }

        const auto muted = i < static_cast<int>(processor.channelMuteStates.size()) && processor.channelMuteStates[i];
        const auto accent = muted ? rgb(DISABLED_PARAM)
                                  : processor.pannerOSC->isConnected() ? toColour(processor.osc_colour)
                                                                       : rgb(M1_ACTION_YELLOW);
        g.setColour(accent);
        g.drawEllipse(point.x - (12.0f * sizeMultiplier), point.y - (12.0f * sizeMultiplier),
                      24.0f * sizeMultiplier, 24.0f * sizeMultiplier, 1.5f);
        g.drawEllipse(point.x - (8.0f * sizeMultiplier), point.y - (8.0f * sizeMultiplier),
                      16.0f * sizeMultiplier, 16.0f * sizeMultiplier, 1.5f);

        g.setFont(juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 4 + (reticleHovered ? 2 : 0))));
        g.drawText(pointNames[i], juce::Rectangle<float>(point.x - 24.0f, point.y - 12.0f, 48.0f, 24.0f).toNearestInt(),
                   juce::Justification::centred, false);
    }
}

bool NativePannerReticleComponent::hitTestAdditionalReticles(juce::Point<float> point)
{
    const auto points = pannerState.m1Encode.getPoints();
    const auto bounds = getLocalBounds().toFloat();

    for (int i = 0; i < pannerState.m1Encode.getPointsCount(); ++i) {
        const auto reticle = juce::Rectangle<float>(20.0f, 20.0f).withCentre({
            (points[i].z + 1.0f) * bounds.getWidth() * 0.5f,
            (-points[i].x + 1.0f) * bounds.getHeight() * 0.5f
        });

        if (reticle.contains(point)) {
            if (i < static_cast<int>(processor.channelMuteStates.size())) {
                processor.channelMuteStates[i] = !processor.channelMuteStates[i];
                repaint();
                return true;
            }
        }
    }

    return false;
}

void NativePannerReticleComponent::startGesture()
{
    auto& params = processor.getValueTreeState();
    if (auto* azimuth = params.getParameter(processor.paramAzimuth)) {
        azimuth->beginChangeGesture();
    }
    if (auto* diverge = params.getParameter(processor.paramDiverge)) {
        diverge->beginChangeGesture();
    }

    processor.azimuthOwnedByUI = true;
    processor.divergeOwnedByUI = true;
}

void NativePannerReticleComponent::endGesture()
{
    auto& params = processor.getValueTreeState();
    if (auto* azimuth = params.getParameter(processor.paramAzimuth)) {
        azimuth->endChangeGesture();
    }
    if (auto* diverge = params.getParameter(processor.paramDiverge)) {
        diverge->endChangeGesture();
    }

    processor.azimuthOwnedByUI = false;
    processor.divergeOwnedByUI = false;
    dragging = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void NativePannerReticleComponent::updateFromPosition(juce::Point<float> localPosition, bool notifyHost)
{
    const auto bounds = getLocalBounds().toFloat();
    float newX = juce::jlimit(-100.0f, 100.0f, ((localPosition.x - bounds.getCentreX()) / (bounds.getWidth() * 0.5f)) * 100.0f);
    float newY = juce::jlimit(-100.0f, 100.0f, (-(localPosition.y - bounds.getCentreY()) / (bounds.getHeight() * 0.5f)) * 100.0f);
    float newAzimuth = 0.0f;
    float newDiverge = 0.0f;
    PannerUIHelpers::convertXYtoRCRaw(newX, newY, newAzimuth, newDiverge);

    pannerState.x = newX;
    pannerState.y = newY;
    pannerState.azimuth = newAzimuth;
    pannerState.diverge = newDiverge;

    if (notifyHost) {
        auto& params = processor.getValueTreeState();
        if (auto* azimuth = params.getParameter(processor.paramAzimuth)) {
            azimuth->setValueNotifyingHost(azimuth->convertTo0to1(newAzimuth));
        }
        if (auto* diverge = params.getParameter(processor.paramDiverge)) {
            diverge->setValueNotifyingHost(diverge->convertTo0to1(newDiverge));
        }
    }

    repaint();
}

void NativePannerReticleComponent::mouseDown(const juce::MouseEvent& event)
{
    if (event.mods.isAltDown() && hitTestAdditionalReticles(event.position)) {
        return;
    }

    const auto reticlePosition = reticlePositionForBounds(getLocalBounds().toFloat());
    if (reticlePosition.getDistanceFrom(event.position) <= 12.0f) {
        dragging = true;
        setMouseCursor(juce::MouseCursor::NoCursor);
        startGesture();
    }
}

void NativePannerReticleComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (!dragging) {
        return;
    }

    updateFromPosition(event.position, true);
}

void NativePannerReticleComponent::mouseUp(const juce::MouseEvent&)
{
    if (dragging) {
        endGesture();
    }
}

void NativePannerReticleComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    startGesture();
    updateFromPosition(event.position, true);
    endGesture();
}

void NativePannerReticleComponent::mouseMove(const juce::MouseEvent& event)
{
    const auto reticlePosition = reticlePositionForBounds(getLocalBounds().toFloat());
    hovered = reticlePosition.getDistanceFrom(event.position) <= 12.0f;
    repaint();
}
