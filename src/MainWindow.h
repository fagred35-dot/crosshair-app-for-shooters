#pragma once

#include "AppSettings.h"
#include "ClickerEngine.h"
#include "CrosshairPreset.h"
#include "OverlayWindow.h"
#include "SettingsStore.h"

#include <windows.h>
#include <shellapi.h>

#include <cstdint>
#include <vector>

namespace aimpoint {

class MainWindow {
public:
    MainWindow(HINSTANCE instance,
               std::vector<CrosshairPreset> presets,
               AppSettings settings,
               SettingsStore& store,
               OverlayWindow& overlay,
               ClickerEngine& clicker);
    ~MainWindow();

    MainWindow(const MainWindow&) = delete;
    MainWindow& operator=(const MainWindow&) = delete;

    [[nodiscard]] bool create(bool startMinimized);
    [[nodiscard]] HWND handle() const noexcept { return window_; }

private:
    enum class Page { Gallery, Crosshair, Clicker, Settings };
    enum class Filter { All, Artistic, Static, Animated, Minimal, Tactical };
    enum class Action {
        None,
        Navigate,
        SelectPreset,
        SetFilter,
        SizeSlider,
        OpacitySlider,
        OffsetXSlider,
        OffsetYSlider,
        ToggleOverlay,
        ToggleOutline,
        ToggleCenterDot,
        ToggleActiveMonitor,
        PreviousPreset,
        RandomPreset,
        NextPreset,
        ToggleClicker,
        CpsSlider,
        VariationSlider,
        PressDurationSlider,
        SetClickMode,
        SetClickButton,
        SetBurst,
        SetHotkey,
        SetStartDelay,
        SetClickLimit,
        ToggleFocusGuard,
        ResetClickCounter,
        ToggleTray,
        ToggleStartup,
        ExitApplication
    };

    struct HitRegion {
        RECT bounds{};
        Action action{Action::None};
        int value{};
    };

    static LRESULT CALLBACK windowProc(HWND window, UINT message, WPARAM wParam, LPARAM lParam);
    LRESULT handleMessage(UINT message, WPARAM wParam, LPARAM lParam);

    void paint();
    void paintSidebar(HDC dc, const RECT& client);
    void paintTopStatus(HDC dc, const RECT& client);
    void paintGallery(HDC dc, const RECT& client);
    void paintCrosshair(HDC dc, const RECT& client);
    void paintClicker(HDC dc, const RECT& client);
    void paintSettings(HDC dc, const RECT& client);

    void addHit(RECT bounds, Action action, int value = 0);
    void drawButton(HDC dc, RECT bounds, const wchar_t* label, Action action, int value = 0, bool accent = false);
    void drawSwitch(HDC dc, int x, int y, const wchar_t* title, const wchar_t* subtitle, bool enabled, Action action);
    void drawSlider(HDC dc,
                    int x,
                    int y,
                    int width,
                    const wchar_t* label,
                    int value,
                    int minimum,
                    int maximum,
                    const wchar_t* suffix,
                    Action action);
    void drawSegment(HDC dc,
                     RECT bounds,
                     const std::vector<const wchar_t*>& labels,
                     int selected,
                     Action action,
                     const std::vector<int>& values = {});
    void drawCrosshairPreview(HDC dc, POINT center, int presetIndex, float scale = 1.0F);

    [[nodiscard]] std::vector<int> filteredPresets() const;
    [[nodiscard]] bool filterMatches(const CrosshairPreset& preset) const;
    void processAction(Action action, int value, POINT point);
    void updateSlider(Action action, int mouseX);
    void applySettings(bool hotkeyChanged = false);
    void registerGlobalHotkeys();
    void unregisterGlobalHotkeys();
    void showMainWindow();
    void addTrayIcon();
    void removeTrayIcon();
    void showTrayMenu();
    void destroyFonts();

    HINSTANCE instance_{};
    HWND window_{};
    std::vector<CrosshairPreset> presets_;
    AppSettings settings_{};
    SettingsStore& store_;
    OverlayWindow& overlay_;
    ClickerEngine& clicker_;

    Page page_{Page::Gallery};
    Filter filter_{Filter::All};
    std::vector<HitRegion> hits_;
    Action dragging_{Action::None};
    RECT draggingBounds_{};
    int galleryScroll_{};
    int galleryMaximumScroll_{};
    std::uint64_t startedAt_{};
    bool closingForExit_{false};
    bool trayAdded_{false};
    bool clickerHotkeyRegistered_{false};

    HFONT logoFont_{};
    HFONT titleFont_{};
    HFONT headingFont_{};
    HFONT bodyFont_{};
    HFONT smallFont_{};
};

} // namespace aimpoint
