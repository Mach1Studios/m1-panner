#pragma once

#ifndef M1_ENABLE_EXTERNAL_RENDERER
#define M1_ENABLE_EXTERNAL_RENDERER 0
#endif

namespace Mach1 {
namespace ExternalRendererModePolicy {

struct Decision
{
    bool geometryEligible = false;
    bool streamToHelper = false;
};

/**
 * Selects the panner processing path from host geometry and the helper's
 * runtime toggle. Only mono/stereo input on a stereo output needs the external
 * renderer; any wider host output stays on the native multichannel path.
 *
 * Keeping this policy independent from JUCE makes both compile-time variants
 * testable without instantiating a plugin host.
 */
constexpr Decision evaluate(int inputChannels, int outputChannels, bool helperEnabled)
{
    const bool eligible = (inputChannels == 1 || inputChannels == 2)
                       && outputChannels == 2;

    return {
        eligible,
        M1_ENABLE_EXTERNAL_RENDERER != 0 && eligible && helperEnabled
    };
}

} // namespace ExternalRendererModePolicy
} // namespace Mach1
