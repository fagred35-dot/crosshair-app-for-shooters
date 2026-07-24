#include "SettingsStore.h"

#include <windows.h>
#include <shlobj.h>

#include <algorithm>
#include <filesystem>
#include <string>

namespace aimpoint {
namespace {

int readInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, const int fallback) {
    return static_cast<int>(GetPrivateProfileIntW(section, key, fallback, path.c_str()));
}

void writeInt(const std::wstring& path, const wchar_t* section, const wchar_t* key, const int value) {
    const std::wstring text = std::to_wstring(value);
    WritePrivateProfileStringW(section, key, text.c_str(), path.c_str());
}

bool writeStartupValue(const bool enabled) {
    HKEY key = nullptr;
    if (RegCreateKeyExW(HKEY_CURRENT_USER,
                        L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",
                        0,
                        nullptr,
                        0,
                        KEY_SET_VALUE,
                        nullptr,
                        &key,
                        nullptr) != ERROR_SUCCESS) {
        return false;
    }

    bool success = false;
    if (enabled) {
        std::wstring executable(32768, L'\0');
        const DWORD length = GetModuleFileNameW(nullptr, executable.data(), static_cast<DWORD>(executable.size()));
        executable.resize(length);
        const std::wstring quoted = L"\"" + executable + L"\" --minimized";
        success = RegSetValueExW(key,
                                 L"AimPoint",
                                 0,
                                 REG_SZ,
                                 reinterpret_cast<const BYTE*>(quoted.c_str()),
                                 static_cast<DWORD>((quoted.size() + 1) * sizeof(wchar_t))) == ERROR_SUCCESS;
    } else {
        const LSTATUS status = RegDeleteValueW(key, L"AimPoint");
        success = status == ERROR_SUCCESS || status == ERROR_FILE_NOT_FOUND;
    }
    RegCloseKey(key);
    return success;
}

} // namespace

SettingsStore::SettingsStore() {
    wchar_t appData[MAX_PATH]{};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, SHGFP_TYPE_CURRENT, appData))) {
        const std::filesystem::path folder = std::filesystem::path(appData) / L"AimPoint";
        std::error_code error;
        std::filesystem::create_directories(folder, error);
        path_ = (folder / L"settings.ini").wstring();
    } else {
        path_ = L"AimPoint.ini";
    }
}

AppSettings SettingsStore::load() const {
    AppSettings value;
    value.selectedPreset = std::clamp(readInt(path_, L"Crosshair", L"Preset", value.selectedPreset), 0, 359);
    value.overlayVisible = readInt(path_, L"Crosshair", L"Visible", value.overlayVisible) != 0;
    value.sizePercent = std::clamp(readInt(path_, L"Crosshair", L"Size", value.sizePercent), 50, 180);
    value.opacityPercent = std::clamp(readInt(path_, L"Crosshair", L"Opacity", value.opacityPercent), 20, 100);
    value.offsetX = std::clamp(readInt(path_, L"Crosshair", L"OffsetX", value.offsetX), -100, 100);
    value.offsetY = std::clamp(readInt(path_, L"Crosshair", L"OffsetY", value.offsetY), -100, 100);
    value.forceOutline = readInt(path_, L"Crosshair", L"Outline", value.forceOutline) != 0;
    value.forceCenterDot = readInt(path_, L"Crosshair", L"CenterDot", value.forceCenterDot) != 0;
    value.followActiveMonitor = readInt(path_, L"Crosshair", L"ActiveMonitor", value.followActiveMonitor) != 0;

    value.clickerEnabled = readInt(path_, L"Clicker", L"Enabled", value.clickerEnabled) != 0;
    value.clicksPerSecond = std::clamp(readInt(path_, L"Clicker", L"CPS", value.clicksPerSecond), 1, 30);
    value.intervalVariationPercent = std::clamp(readInt(path_, L"Clicker", L"Variation", value.intervalVariationPercent), 0, 35);
    value.burstCount = std::clamp(readInt(path_, L"Clicker", L"Burst", value.burstCount), 1, 3);
    value.clickMode = readInt(path_, L"Clicker", L"Mode", 0) == 1 ? ClickMode::Hold : ClickMode::Toggle;
    value.clickButton = readInt(path_, L"Clicker", L"Button", 0) == 1 ? ClickButton::Right : ClickButton::Left;
    const int hotkey = readInt(path_, L"Clicker", L"Hotkey", value.clickerHotkey);
    value.clickerHotkey = std::clamp(hotkey, static_cast<int>(VK_F6), static_cast<int>(VK_F9));

    value.minimizeToTray = readInt(path_, L"Application", L"MinimizeToTray", value.minimizeToTray) != 0;
    value.runAtStartup = readInt(path_, L"Application", L"RunAtStartup", value.runAtStartup) != 0;
    return value;
}

void SettingsStore::save(const AppSettings& value) const {
    writeInt(path_, L"Crosshair", L"Preset", value.selectedPreset);
    writeInt(path_, L"Crosshair", L"Visible", value.overlayVisible);
    writeInt(path_, L"Crosshair", L"Size", value.sizePercent);
    writeInt(path_, L"Crosshair", L"Opacity", value.opacityPercent);
    writeInt(path_, L"Crosshair", L"OffsetX", value.offsetX);
    writeInt(path_, L"Crosshair", L"OffsetY", value.offsetY);
    writeInt(path_, L"Crosshair", L"Outline", value.forceOutline);
    writeInt(path_, L"Crosshair", L"CenterDot", value.forceCenterDot);
    writeInt(path_, L"Crosshair", L"ActiveMonitor", value.followActiveMonitor);

    writeInt(path_, L"Clicker", L"Enabled", value.clickerEnabled);
    writeInt(path_, L"Clicker", L"CPS", value.clicksPerSecond);
    writeInt(path_, L"Clicker", L"Variation", value.intervalVariationPercent);
    writeInt(path_, L"Clicker", L"Burst", value.burstCount);
    writeInt(path_, L"Clicker", L"Mode", static_cast<int>(value.clickMode));
    writeInt(path_, L"Clicker", L"Button", static_cast<int>(value.clickButton));
    writeInt(path_, L"Clicker", L"Hotkey", value.clickerHotkey);

    writeInt(path_, L"Application", L"MinimizeToTray", value.minimizeToTray);
    writeInt(path_, L"Application", L"RunAtStartup", value.runAtStartup);
}

bool SettingsStore::setRunAtStartup(const bool enabled) const {
    return writeStartupValue(enabled);
}

} // namespace aimpoint
