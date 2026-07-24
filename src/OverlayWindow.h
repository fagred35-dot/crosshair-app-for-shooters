#pragma once

#include "AppSettings.h"
#include "CrosshairPreset.h"

#include <windows.h>

#include <cstdint>

namespace aimpoint {

class OverlayWindow {
public:
    explicit OverlayWindow(HINSTANCE instance);
    ~OverlayWindow();

    OverlayWindow(const OverlayWindow&) = delete;
    OverlayWindow& operator=(const OverlayWindow&) = delete;

    [[nodiscard]] bool create();
    void update(const AppSettings& settings,
                const CrosshairPreset& preset,
                std::uint64_t elapsedMilliseconds);
    void setVisible(bool visible);
    [[nodiscard]] HWND handle() const noexcept { return window_; }

private:
    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);
    void paint();
    void reposition();

    HINSTANCE instance_{};
    HWND window_{};
    AppSettings settings_{};
    CrosshairPreset preset_{};
    std::uint64_t elapsedMilliseconds_{};
    bool hasPreset_{false};
};

} // namespace aimpoint
