#include "OverlayWindow.h"

#include "CrosshairRenderer.h"

#include <algorithm>

namespace aimpoint {
namespace {

constexpr wchar_t kOverlayClassName[] = L"AimPointOverlayWindow";
constexpr int kOverlaySize = 280;
constexpr COLORREF kTransparencyKey = RGB(1, 2, 3);

} // namespace

OverlayWindow::OverlayWindow(const HINSTANCE instance) : instance_(instance) {}

OverlayWindow::~OverlayWindow() {
    if (window_) {
        DestroyWindow(window_);
    }
}

bool OverlayWindow::create() {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.hInstance = instance_;
    windowClass.lpfnWndProc = OverlayWindow::windowProc;
    windowClass.lpszClassName = kOverlayClassName;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hbrBackground = CreateSolidBrush(kTransparencyKey);
    RegisterClassExW(&windowClass);

    window_ = CreateWindowExW(WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW |
                                  WS_EX_NOACTIVATE | WS_EX_TOPMOST,
                              kOverlayClassName,
                              L"AimPoint Overlay",
                              WS_POPUP,
                              0,
                              0,
                              kOverlaySize,
                              kOverlaySize,
                              nullptr,
                              nullptr,
                              instance_,
                              this);
    if (!window_) {
        return false;
    }

    SetLayeredWindowAttributes(window_, kTransparencyKey, 255, LWA_COLORKEY | LWA_ALPHA);
    return true;
}

void OverlayWindow::update(const AppSettings& settings,
                           const CrosshairPreset& preset,
                           const std::uint64_t elapsedMilliseconds) {
    settings_ = settings;
    preset_ = preset;
    elapsedMilliseconds_ = elapsedMilliseconds;
    hasPreset_ = true;

    reposition();
    const BYTE alpha = static_cast<BYTE>(std::clamp(settings.opacityPercent, 20, 100) * 255 / 100);
    SetLayeredWindowAttributes(window_, kTransparencyKey, alpha, LWA_COLORKEY | LWA_ALPHA);
    setVisible(settings.overlayVisible);
    InvalidateRect(window_, nullptr, FALSE);
    UpdateWindow(window_);
}

void OverlayWindow::setVisible(const bool visible) {
    if (!window_) {
        return;
    }
    const bool currentlyVisible = IsWindowVisible(window_) != FALSE;
    if (visible && !currentlyVisible) {
        ShowWindow(window_, SW_SHOWNOACTIVATE);
        SetWindowPos(window_, HWND_TOPMOST, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    } else if (!visible && currentlyVisible) {
        ShowWindow(window_, SW_HIDE);
    }
}

void OverlayWindow::reposition() {
    if (!window_) {
        return;
    }

    MONITORINFO monitorInfo{};
    monitorInfo.cbSize = sizeof(monitorInfo);
    HMONITOR monitor = nullptr;
    if (settings_.followActiveMonitor) {
        HWND foreground = GetForegroundWindow();
        monitor = MonitorFromWindow(foreground, MONITOR_DEFAULTTOPRIMARY);
    } else {
        POINT origin{0, 0};
        monitor = MonitorFromPoint(origin, MONITOR_DEFAULTTOPRIMARY);
    }
    GetMonitorInfoW(monitor, &monitorInfo);

    const RECT bounds = monitorInfo.rcMonitor;
    const int centerX = bounds.left + (bounds.right - bounds.left) / 2 + settings_.offsetX;
    const int centerY = bounds.top + (bounds.bottom - bounds.top) / 2 + settings_.offsetY;
    const int targetX = centerX - kOverlaySize / 2;
    const int targetY = centerY - kOverlaySize / 2;
    RECT current{};
    GetWindowRect(window_, &current);
    if (current.left != targetX || current.top != targetY ||
        current.right - current.left != kOverlaySize || current.bottom - current.top != kOverlaySize) {
        SetWindowPos(window_, HWND_TOPMOST,
                     targetX,
                     targetY,
                     kOverlaySize,
                     kOverlaySize,
                     SWP_NOACTIVATE);
    }
}

LRESULT CALLBACK OverlayWindow::windowProc(const HWND window,
                                            const UINT message,
                                            const WPARAM wParam,
                                            const LPARAM lParam) {
    OverlayWindow* self = reinterpret_cast<OverlayWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<OverlayWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handleMessage(message, wParam, lParam)
                : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT OverlayWindow::handleMessage(const UINT message, const WPARAM wParam, const LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_NCHITTEST:
        return HTTRANSPARENT;
    case WM_DESTROY:
        window_ = nullptr;
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void OverlayWindow::paint() {
    PAINTSTRUCT paintStruct{};
    HDC dc = BeginPaint(window_, &paintStruct);
    RECT client{};
    GetClientRect(window_, &client);

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
    const HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
    const HBRUSH background = CreateSolidBrush(kTransparencyKey);
    FillRect(memory, &client, background);
    DeleteObject(background);

    if (hasPreset_) {
        RenderOptions options;
        options.scale = static_cast<float>(settings_.sizePercent) / 100.0F;
        options.outline = settings_.forceOutline;
        options.centerDot = settings_.forceCenterDot;
        options.elapsedMilliseconds = elapsedMilliseconds_;
        CrosshairRenderer::draw(memory,
                                {kOverlaySize / 2, kOverlaySize / 2},
                                preset_,
                                options);
    }

    BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    EndPaint(window_, &paintStruct);
}

} // namespace aimpoint
