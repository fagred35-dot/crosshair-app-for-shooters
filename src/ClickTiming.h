#pragma once

#include "Compat.h"

#include <algorithm>
#include <cmath>

namespace aimpoint {

struct ClickTimingResult {
    int delayMs;
    int nextBurstPosition;
};

// Calculates the delay from one click start to the next. For bursts, short
// intra-burst gaps are compensated by a longer group pause, preserving CPS.
inline ClickTimingResult calculateClickTiming(const int clicksPerSecond,
                                              const int burstCount,
                                              const int currentBurstPosition,
                                              const int variationPercent,
                                              const double randomSample) {
    const int cps = clampValue(clicksPerSecond, 1, 50);
    const int burst = clampValue(burstCount, 1, 3);
    const double baseInterval = 1000.0 / static_cast<double>(cps);
    const double innerInterval = std::max(18.0, std::min(55.0, baseInterval * 0.34));

    int nextPosition = currentBurstPosition + 1;
    double interval = baseInterval;
    if (burst > 1) {
        if (nextPosition < burst) {
            interval = innerInterval;
        } else {
            interval = std::max(baseInterval,
                                baseInterval * static_cast<double>(burst) -
                                    innerInterval * static_cast<double>(burst - 1));
            nextPosition = 0;
        }
    } else {
        nextPosition = 0;
    }

    const double jitter = clampValue(randomSample, -1.0, 1.0) *
                          static_cast<double>(clampValue(variationPercent, 0, 40)) / 100.0;
    interval *= 1.0 + jitter;
    return {std::max(4, static_cast<int>(std::lround(interval))), nextPosition};
}

} // namespace aimpoint
