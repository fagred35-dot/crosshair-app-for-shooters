#pragma once

#include "AppSettings.h"

#include <windows.h>

#include <atomic>
#include <thread>

namespace aimpoint {

inline constexpr UINT WM_AIMPOINT_CLICKER_STATE = WM_APP + 20;

class ClickerEngine {
public:
    ClickerEngine() = default;
    ~ClickerEngine();

    ClickerEngine(const ClickerEngine&) = delete;
    ClickerEngine& operator=(const ClickerEngine&) = delete;

    void start(HWND notificationWindow);
    void stop();
    void configure(const AppSettings& settings);
    void onActivationHotkey();
    void panicStop();

    [[nodiscard]] bool isActive() const noexcept { return active_.load(); }

private:
    void run();
    void setActive(bool active);
    void sendClick() const;

    std::atomic<bool> running_{false};
    std::atomic<bool> enabled_{false};
    std::atomic<bool> active_{false};
    std::atomic<int> clicksPerSecond_{10};
    std::atomic<int> variationPercent_{8};
    std::atomic<int> burstCount_{1};
    std::atomic<int> clickMode_{static_cast<int>(ClickMode::Toggle)};
    std::atomic<int> clickButton_{static_cast<int>(ClickButton::Left)};
    std::atomic<int> hotkey_{VK_F6};
    std::atomic<HWND> notificationWindow_{nullptr};
    std::thread worker_;
};

} // namespace aimpoint
