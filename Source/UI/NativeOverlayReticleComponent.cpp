#include "NativeOverlayReticleComponent.h"

#include "PannerUIHelpers.h"

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

NativeOverlayReticleComponent::NativeOverlayReticleComponent(M1PannerAudioProcessor& processorToUse)
    : processor(processorToUse), pannerState(processorToUse.pannerSettings), monitorState(processorToUse.monitorSettings)
{
    startTimerHz(30);
}

NativeOverlayReticleComponent::~NativeOverlayReticleComponent()
{
    endGesture();
}

void NativeOverlayReticleComponent::timerCallback()
{
    repaint();
}

juce::Point<float> NativeOverlayReticleComponent::reticlePositionForBounds(juce::Rectangle<float> bounds) const
{
    return {
        bounds.getCentreX() + (pannerState.azimuth / 180.0f) * bounds.getWidth() * 0.5f,
        bounds.getCentreY() + (-pannerState.elevation / 90.0f) * bounds.getHeight() * 0.5f
    };
}

void NativeOverlayReticleComponent::paint(juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();
    const auto reticlePosition = reticlePositionForBounds(bounds);
    const bool reticleHovered = dragging || reticlePosition.getDistanceFrom(getMouseXYRelative().toFloat()) <= 12.0f;

    g.fillAll(juce::Colours::transparentBlack);

    g.setColour(rgba(GRID_LINES_1_RGBA));
    g.drawVerticalLine(juce::roundToInt(reticlePosition.x), 0.0f, bounds.getHeight());
    g.drawHorizontalLine(juce::roundToInt(reticlePosition.y), 0.0f, bounds.getWidth());

    g.setColour(rgb(M1_ACTION_YELLOW));
    g.fillEllipse(juce::Rectangle<float>(20.0f + 3.0f * static_cast<float>(reticleHovered),
                                         20.0f + 3.0f * static_cast<float>(reticleHovered))
                      .withCentre(reticlePosition));
    g.setColour(rgb(BACKGROUND_GREY));
    g.fillEllipse(juce::Rectangle<float>(16.0f + 3.0f * static_cast<float>(reticleHovered),
                                         16.0f + 3.0f * static_cast<float>(reticleHovered))
                      .withCentre(reticlePosition));
    g.setColour(rgb(M1_ACTION_YELLOW));
    g.fillEllipse(juce::Rectangle<float>(12.0f, 12.0f).withCentre(reticlePosition));

    const auto points = pannerState.m1Encode.getPoints();
    const auto pointNames = pannerState.m1Encode.getPointsNames();
    for (int i = 0; i < pannerState.m1Encode.getPointsCount(); ++i) {
        if (pointNames[i] == "LFE") {
            continue;
        }

        const auto point = juce::Point<float>(((points[i].z + 1.0f) * 0.5f) * bounds.getWidth(),
                                              ((-points[i].y + 1.0f) * 0.5f) * bounds.getHeight());
        g.setColour(processor.pannerOSC->isConnected() ? toColour(processor.osc_colour) : rgb(M1_ACTION_YELLOW));
        g.drawEllipse(point.x - 10.0f, point.y - 10.0f, 20.0f, 20.0f, 1.5f);
        g.setFont(juce::Font(static_cast<float>(DEFAULT_FONT_SIZE - 4)));
        g.drawText(pointNames[i], juce::Rectangle<float>(point.x - 24.0f, point.y - 10.0f, 48.0f, 20.0f).toNearestInt(),
                   juce::Justification::centred, false);
    }

    if (processor.pannerOSC->isConnected() && monitorState.monitor_mode != 1) {
        const auto yaw = PannerUIHelpers::normalize(monitorState.yaw - 180.0f, -180.0f, 180.0f);
        const auto pitch = monitorState.pitch + 90.0f;
        const float divider = 4.0f;
        const float halfWidth = bounds.getWidth() / (divider * 2.0f);
        const float halfHeight = bounds.getHeight() / (divider * 2.0f);
        const float centerPos = std::fmod(yaw + 0.5f, 1.0f) * bounds.getWidth();
        g.setColour(rgba(OVERLAY_YAW_REF_RGBA));
        g.drawRect(juce::Rectangle<float>(centerPos - halfWidth,
                                          ((pitch / 180.0f) * bounds.getHeight()) - halfHeight,
                                          bounds.getWidth() / divider,
                                          bounds.getHeight() / divider), 1.5f);
    }
}

void NativeOverlayReticleComponent::beginGesture()
{
    auto& params = processor.getValueTreeState();
    if (auto* azimuth = params.getParameter(processor.paramAzimuth)) {
        azimuth->beginChangeGesture();
    }
    if (auto* elevation = params.getParameter(processor.paramElevation)) {
        elevation->beginChangeGesture();
    }

    processor.azimuthOwnedByUI = true;
    processor.elevationOwnedByUI = true;
}

void NativeOverlayReticleComponent::endGesture()
{
    auto& params = processor.getValueTreeState();
    if (auto* azimuth = params.getParameter(processor.paramAzimuth)) {
        azimuth->endChangeGesture();
    }
    if (auto* elevation = params.getParameter(processor.paramElevation)) {
        elevation->endChangeGesture();
    }

    processor.azimuthOwnedByUI = false;
    processor.elevationOwnedByUI = false;
    dragging = false;
    setMouseCursor(juce::MouseCursor::NormalCursor);
}

void NativeOverlayReticleComponent::updateFromPosition(juce::Point<float> localPosition, bool notifyHost)
{
    const auto bounds = getLocalBounds().toFloat();
    const float newAzimuth = ((localPosition.x - bounds.getCentreX()) / (bounds.getWidth() * 0.5f)) * 180.0f;
    const float newElevation = (-(localPosition.y - bounds.getCentreY()) / (bounds.getHeight() * 0.5f)) * 90.0f;

    pannerState.azimuth = newAzimuth;
    pannerState.elevation = newElevation;

    float newX = 0.0f;
    float newY = 0.0f;
    PannerUIHelpers::convertRCtoXYRaw(newAzimuth, pannerState.diverge, newX, newY);
    pannerState.x = newX;
    pannerState.y = newY;

    if (notifyHost) {
        auto& params = processor.getValueTreeState();
        if (auto* azimuth = params.getParameter(processor.paramAzimuth)) {
            azimuth->setValueNotifyingHost(azimuth->convertTo0to1(newAzimuth));
        }
        if (auto* elevation = params.getParameter(processor.paramElevation)) {
            elevation->setValueNotifyingHost(elevation->convertTo0to1(newElevation));
        }
    }

    repaint();
}

void NativeOverlayReticleComponent::mouseDown(const juce::MouseEvent& event)
{
    if (reticlePositionForBounds(getLocalBounds().toFloat()).getDistanceFrom(event.position) <= 12.0f) {
        dragging = true;
        setMouseCursor(juce::MouseCursor::NoCursor);
        beginGesture();
    }
}

void NativeOverlayReticleComponent::mouseDrag(const juce::MouseEvent& event)
{
    if (dragging) {
        updateFromPosition(event.position, true);
    }
}

void NativeOverlayReticleComponent::mouseUp(const juce::MouseEvent&)
{
    if (dragging) {
        endGesture();
    }
}

void NativeOverlayReticleComponent::mouseDoubleClick(const juce::MouseEvent& event)
{
    beginGesture();
    updateFromPosition(event.position, true);
    endGesture();
}
