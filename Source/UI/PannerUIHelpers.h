#pragma once

#include <cmath>
#include <limits>

#include "../Config.h"
#include "../PluginProcessor.h"

namespace PannerUIHelpers {

constexpr float legacyUiScale = 0.7f;

inline juce::Rectangle<int> scaledBounds(int x, int y, int w, int h)
{
    return juce::Rectangle<float>(static_cast<float>(x) * legacyUiScale,
                                  static_cast<float>(y) * legacyUiScale,
                                  static_cast<float>(w) * legacyUiScale,
                                  static_cast<float>(h) * legacyUiScale)
        .getSmallestIntegerContainer();
}

inline void clamp(float& value, float minValue, float maxValue)
{
    value = juce::jlimit(minValue, maxValue, value);
}

inline float normalize(float input, float minValue, float maxValue)
{
    if (juce::approximatelyEqual(minValue, maxValue)) {
        return 0.0f;
    }

    return (input - minValue) / (maxValue - minValue);
}

struct Line2D {
    double x = 0.0;
    double y = 0.0;
    double x2 = 0.0;
    double y2 = 0.0;
};

inline juce::Point<float> linePoint(const Line2D& line)
{
    return { static_cast<float>(line.x), static_cast<float>(line.y) };
}

inline juce::Point<float> lineVector(const Line2D& line)
{
    return { static_cast<float>(line.x2 - line.x), static_cast<float>(line.y2 - line.y) };
}

inline double intersectionFactor(const Line2D& a, const Line2D& b)
{
    const double precision = std::sqrt(std::numeric_limits<double>::epsilon());
    const auto aVector = lineVector(a);
    const auto bVector = lineVector(b);
    const double determinant = aVector.x * bVector.y - aVector.y * bVector.x;

    if (std::abs(determinant) < precision) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto aPoint = linePoint(a);
    const auto bPoint = linePoint(b);
    const double numerator = (bPoint.x - aPoint.x) * bVector.y - (bPoint.y - aPoint.y) * bVector.x;
    return numerator / determinant;
}

inline juce::Point<float> intersectionPoint(const Line2D& a, const Line2D& b)
{
    const auto point = linePoint(a);
    const auto vector = lineVector(a);
    const auto factor = static_cast<float>(intersectionFactor(a, b));
    return point + vector * factor;
}

inline void convertRCtoXYRaw(float r, float d, float& x, float& y)
{
    x = std::cos(juce::degreesToRadians(-r + 90.0f)) * d * std::sqrt(2.0f);
    y = std::sin(juce::degreesToRadians(-r + 90.0f)) * d * std::sqrt(2.0f);

    if (x > 100.0f) {
        const auto intersection = intersectionPoint({ 0, 0, x, y }, { 100, -100, 100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (y > 100.0f) {
        const auto intersection = intersectionPoint({ 0, 0, x, y }, { -100, 100, 100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (x < -100.0f) {
        const auto intersection = intersectionPoint({ 0, 0, x, y }, { -100, -100, -100, 100 });
        x = intersection.x;
        y = intersection.y;
    }
    if (y < -100.0f) {
        const auto intersection = intersectionPoint({ 0, 0, x, y }, { -100, -100, 100, -100 });
        x = intersection.x;
        y = intersection.y;
    }
}

inline void convertXYtoRCRaw(float x, float y, float& r, float& d)
{
    if (juce::approximatelyEqual(x, 0.0f) && juce::approximatelyEqual(y, 0.0f)) {
        r = 0.0f;
        d = 0.0f;
        return;
    }

    d = std::sqrt(x * x + y * y) / std::sqrt(2.0f);
    r = juce::radiansToDegrees(std::atan2(x, y));
}

inline bool supportsElevation(Mach1EncodeInputMode inputMode, int outputChannels)
{
    if (outputChannels <= 4) {
        return false;
    }

    switch (inputMode) {
        case Mach1EncodeInputMode::AFormat:
        case Mach1EncodeInputMode::BFOAACN:
        case Mach1EncodeInputMode::BFOAFUMA:
        case Mach1EncodeInputMode::B2OAACN:
        case Mach1EncodeInputMode::B2OAFUMA:
        case Mach1EncodeInputMode::B3OAACN:
        case Mach1EncodeInputMode::B3OAFUMA:
            return false;
        default:
            return true;
    }
}

inline juce::String inputModeLabel(Mach1EncodeInputMode mode)
{
    switch (mode) {
        case Mach1EncodeInputMode::Mono: return "MONO";
        case Mach1EncodeInputMode::Stereo: return "STEREO";
        case Mach1EncodeInputMode::LCR: return "LCR";
        case Mach1EncodeInputMode::Quad: return "QUAD";
        case Mach1EncodeInputMode::LCRS: return "LCRS";
        case Mach1EncodeInputMode::AFormat: return "AFORMAT";
        case Mach1EncodeInputMode::FiveDotZero: return "5.0 Film";
        case Mach1EncodeInputMode::FiveDotOneFilm: return "5.1 Film";
        case Mach1EncodeInputMode::FiveDotOneDTS: return "5.1 DTS";
        case Mach1EncodeInputMode::FiveDotOneSMTPE: return "5.1 SMPTE";
        case Mach1EncodeInputMode::BFOAACN: return "1OA ACN";
        case Mach1EncodeInputMode::BFOAFUMA: return "1OA FuMa";
        default: return "INPUT";
    }
}

inline juce::String outputModeLabel(Mach1EncodeOutputMode mode)
{
    switch (mode) {
        case Mach1EncodeOutputMode::M1Spatial_4: return "4";
        case Mach1EncodeOutputMode::M1Spatial_8: return "8";
        case Mach1EncodeOutputMode::M1Spatial_14: return "14";
        default: return "OUT";
    }
}

} // namespace PannerUIHelpers
