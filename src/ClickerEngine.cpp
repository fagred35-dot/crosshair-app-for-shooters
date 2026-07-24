#include "ClickerEngine.h"

#include "Compat.h"

#include <algorithm>
#include <chrono>
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
    clicksPerSecond_ = clampValue(settings.clicksPerSecond, 1, 30);
    variationPercent_ = clampValue(settings.intervalVariationPercent, 0, 35);
    burstCount_ = clampValue(settings.burstCount, 1, 3);
    clickMode_ = static_cast<int>(settings.clickMode);
    clickButton_ = static_cast<int>(settings.clickButton);
    hotkey_ = settings.clickerHotkey;
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

void ClickerEngine::setActive(const bool active) {
    const bool previous = active_.exchange(active);
    if (previous != active) {
        const HWND target = notificationWindow_.load();
        if (target) {
            PostMessageW(target, WM_AIMPOINT_CLICKER_STATE, active ? 1 : 0, 0);
        }
    }
}

void ClickerEngine::sendClick() const {
    const bool right = clickButton_.load() == static_cast<int>(ClickButton::Right);
    INPUT inputs[2]{};
    inputs[0].type = INPUT_MOUSE;
    inputs[0].mi.dwFlags = right ? MOUSEEVENTF_RIGHTDOWN : MOUSEEVENTF_LEFTDOWN;
    inputs[1].type = INPUT_MOUSE;
    inputs[1].mi.dwFlags = right ? MOUSEEVENTF_RIGHTUP : MOUSEEVENTF_LEFTUP;
    SendInput(2, inputs, sizeof(INPUT));
}

void ClickerEngine::run() {
    using namespace std::chrono_literals;
    std::mt19937 random(std::random_device{}());
    bool holdWasActive = false;
    bool blockedUntilRelease = false;

    while (running_) {
        const bool escapePressed = (GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0;
        if (escapePressed && active_) {
            setActive(false);
            blockedUntilRelease = true;
        }

        if (!enabled_) {
            setActive(false);
            std::this_thread::sleep_for(8ms);
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
            std::this_thread::sleep_for(4ms);
            continue;
        }

        const int burst = burstCount_.load();
        for (int i = 0; i < burst && running_ && active_; ++i) {
            sendClick();
            if (i + 1 < burst) {
                std::this_thread::sleep_for(18ms);
            }
        }

        const int cps = std::max(1, clicksPerSecond_.load());
        const int baseDelay = 1000 / cps;
        const int spread = baseDelay * variationPercent_.load() / 100;
        std::uniform_int_distribution<int> jitter(-spread, spread);
        const int delay = std::max(8, baseDelay + jitter(random));

        int elapsed = 0;
        while (elapsed < delay && running_ && active_) {
            const int slice = std::min(8, delay - elapsed);
            std::this_thread::sleep_for(std::chrono::milliseconds(slice));
            elapsed += slice;
            if ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) != 0) {
                setActive(false);
                blockedUntilRelease = true;
            }
            if (holdMode && (GetAsyncKeyState(hotkey_.load()) & 0x8000) == 0) {
                setActive(false);
            }
        }
    }
}

} // namespace aimpoint
