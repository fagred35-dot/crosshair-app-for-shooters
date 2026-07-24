#include "ClickerEngine.h"

#include "ClickTiming.h"
#include "Compat.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <random>

namespace aimpoint {

ClickerEngine::~ClickerEngine() {
    stop();
}

void ClickerEngine::start(const HWND notificationWindow) {
    if (running_.exchange(true)) {
        return;
    }
    notificationWindow_ = notificationWindow;
    worker_ = std::thread(&ClickerEngine::run, this);
}

void ClickerEngine::stop() {
    if (!running_.exchange(false)) {
        return;
    }
    if (worker_.joinable()) {
        worker_.join();
    }
    setActive(false);
}

void ClickerEngine::configure(const AppSettings& settings) {
    enabled_ = settings.clickerEnabled;
    clicksPerSecond_ = clampValue(settings.clicksPerSecond, 1, 50);
    variationPercent_ = clampValue(settings.intervalVariationPercent, 0, 40);
    pressDurationMs_ = clampValue(settings.pressDurationMs, 4, 45);
    startDelayMs_ = clampValue(settings.startDelayMs, 0, 3000);
    clickLimit_ = settings.clickLimit == 100 || settings.clickLimit == 500 ||
                          settings.clickLimit == 1000
                      ? settings.clickLimit
                      : 0;
    burstCount_ = clampValue(settings.burstCount, 1, 3);
    clickMode_ = static_cast<int>(settings.clickMode);
    clickButton_ = static_cast<int>(settings.clickButton);
    hotkey_ = settings.clickerHotkey;
    pauseWhileAppFocused_ = settings.pauseWhileAppFocused;
    if (!settings.clickerEnabled) {
        setActive(false);
    }
}

void ClickerEngine::onActivationHotkey() {
    if (!enabled_ || clickMode_ != static_cast<int>(ClickMode::Toggle)) {
        return;
    }
    setActive(!active_.load());
}

void ClickerEngine::panicStop() {
    setActive(false);
}

void ClickerEngine::resetCounter() {
    totalClicks_ = 0;
    sessionClicks_ = 0;
    const HWND target = notificationWindow_.load();
    if (target) {
        PostMessageW(target, WM_AIMPOINT_CLICKER_STATE, 0, 0);
    }
}

void ClickerEngine::setActive(const bool active) {
    const bool previous = active_.exchange(active);
    if (!active) {
        setPaused(false);
    }
    if (previous != active) {
        const HWND target = notificationWindow_.load();
        if (target) {
            PostMessageW(target, WM_AIMPOINT_CLICKER_STATE, active ? 1 : 0, 0);
        }
    }
}

void ClickerEngine::setPaused(const bool paused) {
    const bool previous = paused_.exchange(paused);
    if (previous != paused) {
        const HWND target = notificationWindow_.load();
        if (target) {
            PostMessageW(target, WM_AIMPOINT_CLICKER_STATE, paused ? 2 : (active_ ? 1 : 0), 0);
        }
    }
}

bool ClickerEngine::sendMouseEvent(const bool down) const {
    const bool right = clickButton_.load() == static_cast<int>(ClickButton::Right);
    INPUT input{};
    input.type = INPUT_MOUSE;
    if (right) {
        input.mi.dwFlags = down ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_RIGHTUP;
    } else {
        input.mi.dwFlags = down ? MOUSEEVENTF_LEFTDOWN : MOUSEEVENTF_LEFTUP;
    }
    input.mi.dwExtraInfo = static_cast<ULONG_PTR>(0xA17C11C);
    return SendInput(1, &input, sizeof(INPUT)) == 1;
}

bool ClickerEngine::waitResponsive(const int durationMs,
                                   const bool holdMode,
                                   bool& blockedUntilRelease) {
    using namespace std::chrono;
    const steady_clock::time_point deadline = steady_clock::now() +
                                               std::chrono::milliseconds(std::max(0, durationMs));
    while (running_ && steady_clock::now() < deadline) {
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
            setActive(false);
            blockedUntilRelease = true;
            return false;
        }
        if (!enabled_) {
            setActive(false);
            return false;
        }
        if (holdMode && (GetAsyncKeyState(hotkey_.load()) & 0x8000) == 0) {
            setActive(false);
            return false;
        }
        if (!active_) {
            return false;
        }
        const milliseconds remaining = duration_cast<std::chrono::milliseconds>(deadline - steady_clock::now());
        const int sleepTime = std::max(1, std::min(3, static_cast<int>(remaining.count())));
        std::this_thread::sleep_for(std::chrono::milliseconds(sleepTime));
    }
    return running_ && active_;
}

void ClickerEngine::run() {
    using namespace std::chrono;
    std::mt19937 random(std::random_device{}());
    std::uniform_real_distribution<double> unit(-1.0, 1.0);

    bool holdWasActive = false;
    bool blockedUntilRelease = false;
    bool activationInitialized = false;
    bool focusWasPaused = false;
    int burstPosition = 0;
    steady_clock::time_point nextClick = steady_clock::now();

    while (running_) {
        if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0 && active_) {
            setActive(false);
            blockedUntilRelease = true;
        }

        if (!enabled_) {
            setActive(false);
            activationInitialized = false;
            std::this_thread::sleep_for(milliseconds(8));
            continue;
        }

        const bool holdMode = clickMode_ == static_cast<int>(ClickMode::Hold);
        if (holdMode) {
            const bool keyDown = (GetAsyncKeyState(hotkey_.load()) & 0x8000) != 0;
            if (!keyDown) {
                blockedUntilRelease = false;
            }
            const bool shouldBeActive = keyDown && !blockedUntilRelease;
            if (shouldBeActive != holdWasActive) {
                setActive(shouldBeActive);
                holdWasActive = shouldBeActive;
            }
        } else {
            holdWasActive = false;
        }

        if (!active_) {
            activationInitialized = false;
            focusWasPaused = false;
            burstPosition = 0;
            setPaused(false);
            std::this_thread::sleep_for(milliseconds(4));
            continue;
        }

        const HWND ownWindow = notificationWindow_.load();
        const bool focusBlocked = pauseWhileAppFocused_ && ownWindow && GetForegroundWindow() == ownWindow;
        if (focusBlocked) {
            setPaused(true);
            focusWasPaused = true;
            std::this_thread::sleep_for(milliseconds(8));
            continue;
        }

        if (!activationInitialized || focusWasPaused) {
            sessionClicks_ = activationInitialized ? sessionClicks_.load() : 0;
            burstPosition = 0;
            nextClick = steady_clock::now() + milliseconds(startDelayMs_.load());
            activationInitialized = true;
            focusWasPaused = false;
        }
        setPaused(false);

        const steady_clock::time_point now = steady_clock::now();
        if (now < nextClick) {
            const int remaining = static_cast<int>(duration_cast<milliseconds>(nextClick - now).count());
            waitResponsive(std::min(remaining, 6), holdMode, blockedUntilRelease);
            continue;
        }

        const int currentCps = std::max(1, clicksPerSecond_.load());
        const int effectivePress = std::min(pressDurationMs_.load(), std::max(4, 1000 / currentCps - 2));
        const bool downSent = sendMouseEvent(true);
        waitResponsive(effectivePress, holdMode, blockedUntilRelease);
        const bool upSent = sendMouseEvent(false); // Always release the button, even after a panic stop.
        if (downSent && upSent) {
            totalClicks_.fetch_add(1);
            const std::uint64_t session = sessionClicks_.fetch_add(1) + 1;
            const int limit = clickLimit_.load();
            if (limit > 0 && session >= static_cast<std::uint64_t>(limit)) {
                setActive(false);
                if (holdMode) {
                    blockedUntilRelease = true;
                }
                continue;
            }
        }

        // Average two uniform values: a triangular distribution feels less mechanical
        // while avoiding the extreme pauses produced by a plain uniform jitter.
        const double triangular = (unit(random) + unit(random)) * 0.5;
        const ClickTimingResult timing = calculateClickTiming(currentCps,
                                                               burstCount_.load(),
                                                               burstPosition,
                                                               variationPercent_.load(),
                                                               triangular);
        burstPosition = timing.nextBurstPosition;
        nextClick += milliseconds(timing.delayMs);
        if (nextClick + milliseconds(250) < steady_clock::now()) {
            nextClick = steady_clock::now() + milliseconds(1);
        }
    }
}

} // namespace aimpoint
