// Regression tests for the panner's "/m1-channel-config" handling policy.
//
// Guards against a field-reported failure mode: the old handler compared the
// broadcast channel count against the panner's INPUT channel count, which
// practically never matched, so every "/m1-channel-config" broadcast caused a
// host-visible output-mode parameter change in every panner instance. With
// many instances this flooded the host's message thread during session load
// (grey/unresponsive plugin UIs).

#include "ChannelConfigPolicy.h"
#include "ExternalRendererModePolicy.h"

#include <iostream>

static int failures = 0;

#define CHECK(cond)                                                                   \
    do {                                                                              \
        if (!(cond)) {                                                                \
            ++failures;                                                               \
            std::cout << "FAILED: " #cond " (" << __FILE__ << ":" << __LINE__ << ")"  \
                      << std::endl;                                                   \
        }                                                                             \
    } while (0)

int main()
{
    using namespace Mach1::ChannelConfigPolicy;

    // Channel-count mapping: only the Mach1 spatial layouts are accepted.
    CHECK(outputModeForChannelCount(4) == static_cast<int>(M1Spatial_4));
    CHECK(outputModeForChannelCount(8) == static_cast<int>(M1Spatial_8));
    CHECK(outputModeForChannelCount(14) == static_cast<int>(M1Spatial_14));

    CHECK(outputModeForChannelCount(0) == kNoOutputMode);
    CHECK(outputModeForChannelCount(1) == kNoOutputMode);
    CHECK(outputModeForChannelCount(2) == kNoOutputMode);
    CHECK(outputModeForChannelCount(6) == kNoOutputMode);
    CHECK(outputModeForChannelCount(16) == kNoOutputMode);
    CHECK(outputModeForChannelCount(-1) == kNoOutputMode);

    const int mode8 = outputModeForChannelCount(8);

    // A locked output layout must never be changed by a broadcast.
    CHECK(!shouldApplyChannelConfig(true, mode8, 0.0f, 0.5f));

    // Unrecognized channel counts must never touch the parameter.
    CHECK(!shouldApplyChannelConfig(false, kNoOutputMode, 0.0f, 0.5f));

    // Regression: if the parameter already holds the target value, the host
    // must NOT be notified again (this is what made registration broadcasts
    // O(N^2) host-visible parameter churn).
    CHECK(!shouldApplyChannelConfig(false, mode8, 0.5f, 0.5f));

    // A real change must go through.
    CHECK(shouldApplyChannelConfig(false, mode8, 0.0f, 0.5f));

    using Mach1::ExternalRendererModePolicy::evaluate;

    // Stereo-only host layouts are the only external-renderer candidates.
    CHECK(evaluate(1, 2, true).geometryEligible);
    CHECK(evaluate(2, 2, true).geometryEligible);
    CHECK(!evaluate(1, 1, true).geometryEligible);
    CHECK(!evaluate(2, 4, true).geometryEligible);
    CHECK(!evaluate(4, 4, true).geometryEligible);
    CHECK(!evaluate(8, 8, true).geometryEligible);
    CHECK(!evaluate(14, 14, true).geometryEligible);

    // The helper toggle can disable an eligible layout, but it must never
    // turn a native multichannel layout into a streaming one.
    CHECK(!evaluate(2, 2, false).streamToHelper);
    CHECK(!evaluate(4, 4, true).streamToHelper);
    CHECK(!evaluate(8, 8, true).streamToHelper);
    CHECK(!evaluate(14, 14, true).streamToHelper);

#if M1_ENABLE_EXTERNAL_RENDERER
    CHECK(evaluate(1, 2, true).streamToHelper);
    CHECK(evaluate(2, 2, true).streamToHelper);
#else
    // Native-only build: even eligible stereo geometry must never stream.
    CHECK(!evaluate(1, 2, true).streamToHelper);
    CHECK(!evaluate(2, 2, true).streamToHelper);
#endif

    if (failures == 0) {
        std::cout << "All m1-panner policy tests passed" << std::endl;
        return 0;
    }

    std::cout << failures << " check(s) failed" << std::endl;
    return 1;
}
