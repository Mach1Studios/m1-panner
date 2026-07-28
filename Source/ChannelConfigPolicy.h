#pragma once

#include "Mach1EncodeCAPI.h"
#include <cmath>

namespace Mach1 {
namespace ChannelConfigPolicy {

// Sentinel returned when a broadcast channel count does not map to a Mach1
// spatial output layout (e.g. a monitor in a stereo/passthrough state).
constexpr int kNoOutputMode = -1;

/**
 * Maps the system/monitor channel count broadcast by m1-system-helper
 * ("/m1-channel-config") to the panner output mode parameter value.
 *
 * Returns kNoOutputMode for unrecognized channel counts; callers must ignore
 * those broadcasts instead of touching the output-mode parameter.
 */
inline int outputModeForChannelCount(int channelCount)
{
    switch (channelCount) {
        case 4: return static_cast<int>(Mach1EncodeOutputMode::M1Spatial_4);
        case 8: return static_cast<int>(Mach1EncodeOutputMode::M1Spatial_8);
        case 14: return static_cast<int>(Mach1EncodeOutputMode::M1Spatial_14);
        default: return kNoOutputMode;
    }
}

/**
 * Decides whether a "/m1-channel-config" broadcast should result in a
 * host-visible parameter change (setValueNotifyingHost).
 *
 * Regression guard: the previous implementation compared the broadcast channel
 * count against the panner's INPUT channel count, which practically never
 * matched, so every broadcast triggered a host parameter change in every
 * panner instance. With many instances this flooded the host's message thread
 * and automation system (grey/unresponsive plugin UIs at session load).
 *
 * @param outputLayoutLocked       the user's output-layout lock toggle
 * @param targetOutputMode         result of outputModeForChannelCount()
 * @param currentNormalizedValue   current normalized value of the output-mode parameter
 * @param targetNormalizedValue    normalized value the parameter would be set to
 */
inline bool shouldApplyChannelConfig(bool outputLayoutLocked,
                                     int targetOutputMode,
                                     float currentNormalizedValue,
                                     float targetNormalizedValue)
{
    if (outputLayoutLocked)
        return false;
    if (targetOutputMode == kNoOutputMode)
        return false;
    // Only notify the host when the parameter actually changes.
    return std::fabs(currentNormalizedValue - targetNormalizedValue) > 1.0e-6f;
}

} // namespace ChannelConfigPolicy
} // namespace Mach1
