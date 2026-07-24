#include "ClickerEngine.h"
#include "CrosshairPreset.h"
#include "MainWindow.h"
#include "OverlayWindow.h"
#include "SettingsStore.h"

#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

#include <exception>
#include <string>
#include <utility>

int WINAPI wWinMain(const HINSTANCE instance, HINSTANCE, PWSTR, int) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HANDLE singleInstance = CreateMutexW(nullptr, TRUE, L"Local\\AimPoint.SingleInstance.v1");
    if (singleInstance && GetLastError() == ERROR_ALREADY_EXISTS) {
        if (HWND existing = FindWindowW(L"AimPointMainWindow", nullptr)) {
            ShowWindow(existing, SW_SHOW);
            ShowWindow(existing, SW_RESTORE);
            SetForegroundWindow(existing);
        }
        CloseHandle(singleInstance);
        return 0;
    }

    INITCOMMONCONTROLSEX controls{sizeof(controls), ICC_STANDARD_CLASSES};
    InitCommonControlsEx(&controls);

    bool startMinimized = false;
    int argumentCount = 0;
    if (wchar_t** arguments = CommandLineToArgvW(GetCommandLineW(), &argumentCount)) {
        for (int index = 1; index < argumentCount; ++index) {
            if (std::wstring(arguments[index]) == L"--minimized") {
                startMinimized = true;
            }
        }
        LocalFree(arguments);
    }

    int exitCode = 0;
    try {
        aimpoint::SettingsStore store;
        auto settings = store.load();
        auto presets = aimpoint::makePresetCatalog();
        aimpoint::OverlayWindow overlay(instance);
        aimpoint::ClickerEngine clicker;

        if (!overlay.create()) {
            MessageBoxW(nullptr, L"Не удалось создать окно оверлея.", L"AimPoint", MB_OK | MB_ICONERROR);
            exitCode = 1;
        } else {
            aimpoint::MainWindow mainWindow(instance, std::move(presets), settings, store, overlay, clicker);
            if (!mainWindow.create(startMinimized)) {
                MessageBoxW(nullptr, L"Не удалось запустить AimPoint.", L"AimPoint", MB_OK | MB_ICONERROR);
                exitCode = 1;
            } else {
                MSG message{};
                while (GetMessageW(&message, nullptr, 0, 0) > 0) {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                exitCode = static_cast<int>(message.wParam);
            }
        }
    } catch (const std::exception&) {
        MessageBoxW(nullptr, L"Произошла непредвиденная ошибка.", L"AimPoint", MB_OK | MB_ICONERROR);
        exitCode = 1;
    }

    if (singleInstance) {
        ReleaseMutex(singleInstance);
        CloseHandle(singleInstance);
    }
    return exitCode;
}
