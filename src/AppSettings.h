#pragma once

namespace aimpoint {

enum class ClickMode {
    Toggle = 0,
    Hold = 1
};

enum class ClickButton {
    Left = 0,
    Right = 1
};

struct AppSettings {
    int selectedPreset{360}; // Butterfly · Neon · Static
    bool overlayVisible{true};
    int sizePercent{100};
    int opacityPercent{100};
    int offsetX{0};
    int offsetY{0};
    bool forceOutline{true};
    bool forceCenterDot{false};
    bool followActiveMonitor{true};

    bool clickerEnabled{false};
    int clicksPerSecond{10};
    int intervalVariationPercent{8};
    int pressDurationMs{12};
    int startDelayMs{0};
    int clickLimit{0}; // 0 = unlimited
    int burstCount{1};
    ClickMode clickMode{ClickMode::Toggle};
    ClickButton clickButton{ClickButton::Left};
    int clickerHotkey{0x75}; // VK_F6
    bool pauseWhileAppFocused{true};

    bool minimizeToTray{true};
    bool runAtStartup{false};
};

} // namespace aimpoint
