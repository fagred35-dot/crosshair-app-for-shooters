#include "ClickTiming.h"

#include <cassert>
#include <iostream>

int main() {
    const auto steady = aimpoint::calculateClickTiming(10, 1, 0, 0, 0.0);
    assert(steady.delayMs == 100);
    assert(steady.nextBurstPosition == 0);

    int position = 0;
    int burstCycleDuration = 0;
    for (int click = 0; click < 3; ++click) {
        const auto timing = aimpoint::calculateClickTiming(10, 3, position, 0, 0.0);
        burstCycleDuration += timing.delayMs;
        position = timing.nextBurstPosition;
    }
    assert(position == 0);
    assert(burstCycleDuration == 300); // 3 clicks at an overall 10 CPS.

    const auto faster = aimpoint::calculateClickTiming(50, 1, 0, 0, 0.0);
    assert(faster.delayMs == 20);

    const auto lowJitter = aimpoint::calculateClickTiming(10, 1, 0, 40, -1.0);
    const auto highJitter = aimpoint::calculateClickTiming(10, 1, 0, 40, 1.0);
    assert(lowJitter.delayMs == 60);
    assert(highJitter.delayMs == 140);

    std::cout << "Click timing: steady, burst CPS preservation, and jitter limits passed\n";
    return 0;
}
