#include "MainWindow.h"

#include "Compat.h"
#include "CrosshairRenderer.h"
#include "Resource.h"

#include <windowsx.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <random>
#include <string>

namespace aimpoint {
namespace {

constexpr wchar_t kMainClassName[] = L"AimPointMainWindow";
constexpr UINT kTrayMessage = WM_APP + 21;
constexpr UINT_PTR kAnimationTimer = 1;
constexpr int kClickerHotkeyId = 1001;
constexpr int kOverlayHotkeyId = 1002;
constexpr int kTrayShow = 5001;
constexpr int kTrayOverlay = 5002;
constexpr int kTrayExit = 5003;
constexpr int kSidebarWidth = 220;

constexpr COLORREF kBackground = RGB(14, 17, 23);
constexpr COLORREF kSidebar = RGB(18, 22, 30);
constexpr COLORREF kCard = RGB(24, 29, 39);
constexpr COLORREF kCardHover = RGB(29, 35, 47);
constexpr COLORREF kBorder = RGB(43, 50, 65);
constexpr COLORREF kText = RGB(239, 243, 249);
constexpr COLORREF kMuted = RGB(139, 150, 169);
constexpr COLORREF kAccent = RGB(78, 231, 166);
constexpr COLORREF kAccentDark = RGB(28, 102, 77);
constexpr COLORREF kWarning = RGB(255, 186, 73);
constexpr COLORREF kDanger = RGB(255, 97, 111);

void fillSolid(HDC dc, const RECT rect, const COLORREF color) {
    const HBRUSH brush = CreateSolidBrush(color);
    FillRect(dc, &rect, brush);
    DeleteObject(brush);
}

void fillRound(HDC dc, const RECT rect, const int radius, const COLORREF color) {
    const HBRUSH brush = CreateSolidBrush(color);
    const HGDIOBJ oldBrush = SelectObject(dc, brush);
    const HGDIOBJ oldPen = SelectObject(dc, GetStockObject(NULL_PEN));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldPen);
    SelectObject(dc, oldBrush);
    DeleteObject(brush);
}

void strokeRound(HDC dc, const RECT rect, const int radius, const COLORREF color, const int width = 1) {
    const HPEN pen = CreatePen(PS_SOLID, width, color);
    const HGDIOBJ oldPen = SelectObject(dc, pen);
    const HGDIOBJ oldBrush = SelectObject(dc, GetStockObject(HOLLOW_BRUSH));
    RoundRect(dc, rect.left, rect.top, rect.right, rect.bottom, radius, radius);
    SelectObject(dc, oldBrush);
    SelectObject(dc, oldPen);
    DeleteObject(pen);
}

void drawText(HDC dc,
              const std::wstring& value,
              RECT rect,
              const HFONT font,
              const COLORREF color,
              const UINT format = DT_LEFT | DT_VCENTER | DT_SINGLELINE) {
    const HGDIOBJ oldFont = SelectObject(dc, font);
    SetBkMode(dc, TRANSPARENT);
    SetTextColor(dc, color);
    DrawTextW(dc, value.c_str(), static_cast<int>(value.size()), &rect, format);
    SelectObject(dc, oldFont);
}

bool pointInside(const RECT& rect, const POINT point) {
    return PtInRect(&rect, point) != FALSE;
}

std::wstring hotkeyName(const int virtualKey) {
    if (virtualKey >= VK_F1 && virtualKey <= VK_F24) {
        return L"F" + std::to_wstring(virtualKey - VK_F1 + 1);
    }
    return L"?";
}

} // namespace

MainWindow::MainWindow(const HINSTANCE instance,
                       std::vector<CrosshairPreset> presets,
                       const AppSettings settings,
                       SettingsStore& store,
                       OverlayWindow& overlay,
                       ClickerEngine& clicker)
    : instance_(instance),
      presets_(std::move(presets)),
      settings_(settings),
      store_(store),
      overlay_(overlay),
      clicker_(clicker) {}

MainWindow::~MainWindow() {
    if (window_) {
        closingForExit_ = true;
        DestroyWindow(window_);
    }
    destroyFonts();
}

bool MainWindow::create(const bool startMinimized) {
    WNDCLASSEXW windowClass{};
    windowClass.cbSize = sizeof(windowClass);
    windowClass.style = CS_HREDRAW | CS_VREDRAW;
    windowClass.lpfnWndProc = MainWindow::windowProc;
    windowClass.hInstance = instance_;
    windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    windowClass.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_AIMPOINT));
    windowClass.hbrBackground = CreateSolidBrush(kBackground);
    windowClass.lpszClassName = kMainClassName;
    RegisterClassExW(&windowClass);

    RECT desired{0, 0, 1120, 740};
    AdjustWindowRectEx(&desired, WS_OVERLAPPEDWINDOW, FALSE, 0);
    const int width = desired.right - desired.left;
    const int height = desired.bottom - desired.top;
    const int x = (GetSystemMetrics(SM_CXSCREEN) - width) / 2;
    const int y = (GetSystemMetrics(SM_CYSCREEN) - height) / 2;

    window_ = CreateWindowExW(0,
                              kMainClassName,
                              L"AimPoint — Прицелы и автокликер",
                              WS_OVERLAPPEDWINDOW,
                              x,
                              y,
                              width,
                              height,
                              nullptr,
                              nullptr,
                              instance_,
                              this);
    if (!window_) {
        return false;
    }

    logoFont_ = CreateFontW(-25, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH, L"Segoe UI");
    titleFont_ = CreateFontW(-30, 0, 0, 0, FW_BOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");
    headingFont_ = CreateFontW(-20, 0, 0, 0, FW_SEMIBOLD, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                               OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                               DEFAULT_PITCH, L"Segoe UI");
    bodyFont_ = CreateFontW(-16, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                            DEFAULT_PITCH, L"Segoe UI");
    smallFont_ = CreateFontW(-13, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
                             OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY,
                             DEFAULT_PITCH, L"Segoe UI");

    startedAt_ = GetTickCount64();
    addTrayIcon();
    registerGlobalHotkeys();
    clicker_.configure(settings_);
    clicker_.start(window_);
    overlay_.update(settings_, presets_.at(static_cast<std::size_t>(settings_.selectedPreset)), 0);
    SetTimer(window_, kAnimationTimer, 50, nullptr);

    if (!startMinimized) {
        ShowWindow(window_, SW_SHOW);
        UpdateWindow(window_);
    }
    return true;
}

LRESULT CALLBACK MainWindow::windowProc(const HWND window,
                                         const UINT message,
                                         const WPARAM wParam,
                                         const LPARAM lParam) {
    MainWindow* self = reinterpret_cast<MainWindow*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        const auto* create = reinterpret_cast<CREATESTRUCTW*>(lParam);
        self = static_cast<MainWindow*>(create->lpCreateParams);
        self->window_ = window;
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(self));
    }
    return self ? self->handleMessage(message, wParam, lParam)
                : DefWindowProcW(window, message, wParam, lParam);
}

LRESULT MainWindow::handleMessage(const UINT message, const WPARAM wParam, const LPARAM lParam) {
    switch (message) {
    case WM_PAINT:
        paint();
        return 0;
    case WM_ERASEBKGND:
        return 1;
    case WM_GETMINMAXINFO: {
        auto* info = reinterpret_cast<MINMAXINFO*>(lParam);
        info->ptMinTrackSize = {980, 650};
        return 0;
    }
    case WM_LBUTTONDOWN: {
        const POINT point{GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam)};
        for (auto iterator = hits_.rbegin(); iterator != hits_.rend(); ++iterator) {
            if (pointInside(iterator->bounds, point)) {
                if (iterator->action == Action::SizeSlider || iterator->action == Action::OpacitySlider ||
                    iterator->action == Action::OffsetXSlider || iterator->action == Action::OffsetYSlider ||
                    iterator->action == Action::CpsSlider || iterator->action == Action::VariationSlider) {
                    dragging_ = iterator->action;
                    draggingBounds_ = iterator->bounds;
                    SetCapture(window_);
                }
                processAction(iterator->action, iterator->value, point);
                break;
            }
        }
        return 0;
    }
    case WM_MOUSEMOVE:
        if (dragging_ != Action::None && GetCapture() == window_) {
            updateSlider(dragging_, GET_X_LPARAM(lParam));
        }
        return 0;
    case WM_LBUTTONUP:
        if (dragging_ != Action::None) {
            dragging_ = Action::None;
            ReleaseCapture();
            store_.save(settings_);
        }
        return 0;
    case WM_MOUSEWHEEL:
        if (page_ == Page::Gallery) {
            const int steps = GET_WHEEL_DELTA_WPARAM(wParam) / WHEEL_DELTA;
            galleryScroll_ = clampValue(galleryScroll_ - steps * 95, 0, galleryMaximumScroll_);
            InvalidateRect(window_, nullptr, FALSE);
        }
        return 0;
    case WM_TIMER:
        if (wParam == kAnimationTimer) {
            const std::uint64_t elapsed = GetTickCount64() - startedAt_;
            overlay_.update(settings_, presets_.at(static_cast<std::size_t>(settings_.selectedPreset)), elapsed);
            if (page_ == Page::Gallery || page_ == Page::Crosshair || page_ == Page::Clicker) {
                InvalidateRect(window_, nullptr, FALSE);
            }
        }
        return 0;
    case WM_HOTKEY:
        if (wParam == kClickerHotkeyId) {
            clicker_.onActivationHotkey();
            InvalidateRect(window_, nullptr, FALSE);
        } else if (wParam == kOverlayHotkeyId) {
            settings_.overlayVisible = !settings_.overlayVisible;
            applySettings();
        }
        return 0;
    case WM_AIMPOINT_CLICKER_STATE:
        InvalidateRect(window_, nullptr, FALSE);
        return 0;
    case WM_SIZE:
        InvalidateRect(window_, nullptr, FALSE);
        return 0;
    case WM_CLOSE:
        if (settings_.minimizeToTray && !closingForExit_) {
            ShowWindow(window_, SW_HIDE);
        } else {
            DestroyWindow(window_);
        }
        return 0;
    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case kTrayShow:
            showMainWindow();
            break;
        case kTrayOverlay:
            settings_.overlayVisible = !settings_.overlayVisible;
            applySettings();
            break;
        case kTrayExit:
            closingForExit_ = true;
            DestroyWindow(window_);
            break;
        default:
            break;
        }
        return 0;
    case kTrayMessage: {
        const UINT event = LOWORD(lParam);
        if (event == WM_LBUTTONDBLCLK) {
            showMainWindow();
        } else if (event == WM_RBUTTONUP || event == WM_CONTEXTMENU) {
            showTrayMenu();
        }
        return 0;
    }
    case WM_DESTROY:
        KillTimer(window_, kAnimationTimer);
        unregisterGlobalHotkeys();
        removeTrayIcon();
        clicker_.stop();
        window_ = nullptr;
        PostQuitMessage(0);
        return 0;
    default:
        return DefWindowProcW(window_, message, wParam, lParam);
    }
}

void MainWindow::paint() {
    PAINTSTRUCT paintStruct{};
    HDC dc = BeginPaint(window_, &paintStruct);
    RECT client{};
    GetClientRect(window_, &client);

    HDC memory = CreateCompatibleDC(dc);
    HBITMAP bitmap = CreateCompatibleBitmap(dc, client.right, client.bottom);
    const HGDIOBJ oldBitmap = SelectObject(memory, bitmap);
    fillSolid(memory, client, kBackground);
    hits_.clear();

    paintSidebar(memory, client);
    paintTopStatus(memory, client);
    switch (page_) {
    case Page::Gallery:
        paintGallery(memory, client);
        break;
    case Page::Crosshair:
        paintCrosshair(memory, client);
        break;
    case Page::Clicker:
        paintClicker(memory, client);
        break;
    case Page::Settings:
        paintSettings(memory, client);
        break;
    }

    BitBlt(dc, 0, 0, client.right, client.bottom, memory, 0, 0, SRCCOPY);
    SelectObject(memory, oldBitmap);
    DeleteObject(bitmap);
    DeleteDC(memory);
    EndPaint(window_, &paintStruct);
}

void MainWindow::paintSidebar(HDC dc, const RECT& client) {
    fillSolid(dc, {0, 0, kSidebarWidth, client.bottom}, kSidebar);
    fillRound(dc, {22, 24, 62, 64}, 13, kAccent);
    const HPEN logoPen = CreatePen(PS_SOLID, 2, RGB(10, 48, 37));
    const HGDIOBJ oldPen = SelectObject(dc, logoPen);
    MoveToEx(dc, 31, 44, nullptr);
    LineTo(dc, 53, 44);
    MoveToEx(dc, 42, 33, nullptr);
    LineTo(dc, 42, 55);
    Ellipse(dc, 35, 37, 49, 51);
    SelectObject(dc, oldPen);
    DeleteObject(logoPen);
    drawText(dc, L"AimPoint", {72, 22, 198, 52}, logoFont_, kText);
    drawText(dc, L"CONTROL CENTER", {73, 48, 200, 68}, smallFont_, kMuted);

    const auto drawNavigation = [&](const int y, const wchar_t* number, const wchar_t* label, const Page page) {
        const bool active = page_ == page;
        const RECT bounds{16, y, 204, y + 50};
        if (active) {
            fillRound(dc, bounds, 12, kCardHover);
            fillRound(dc, {16, y + 10, 20, y + 40}, 3, kAccent);
        }
        drawText(dc, number, {32, y, 57, y + 50}, headingFont_, active ? kAccent : kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        drawText(dc, label, {68, y, 194, y + 50}, bodyFont_, active ? kText : kMuted);
        addHit(bounds, Action::Navigate, static_cast<int>(page));
    };

    drawNavigation(108, L"01", L"Галерея", Page::Gallery);
    drawNavigation(166, L"02", L"Мой прицел", Page::Crosshair);
    drawNavigation(224, L"03", L"Автокликер", Page::Clicker);
    drawNavigation(282, L"04", L"Настройки", Page::Settings);

    const int statusTop = std::max(380, static_cast<int>(client.bottom) - 126);
    const RECT status{16, statusTop, 204, statusTop + 76};
    fillRound(dc, status, 12, kCard);
    fillRound(dc, {30, statusTop + 17, 40, statusTop + 27}, 10,
              settings_.overlayVisible ? kAccent : kMuted);
    drawText(dc, settings_.overlayVisible ? L"Прицел включён" : L"Прицел скрыт",
             {50, statusTop + 8, 194, statusTop + 38}, bodyFont_, kText);
    drawText(dc, L"Ctrl + Shift + O", {30, statusTop + 39, 194, statusTop + 65}, smallFont_, kMuted);
    addHit(status, Action::ToggleOverlay);
    drawText(dc, L"AimPoint 1.0  •  360 пресетов", {22, client.bottom - 37, 204, client.bottom - 12},
             smallFont_, kMuted);
}

void MainWindow::paintTopStatus(HDC dc, const RECT& client) {
    const bool clickerActive = clicker_.isActive();
    const int right = client.right - 30;
    const RECT clickerPill{right - 142, 27, right, 59};
    fillRound(dc, clickerPill, 16, clickerActive ? RGB(32, 88, 68) : kCard);
    fillRound(dc, {clickerPill.left + 13, 38, clickerPill.left + 21, 46}, 8,
              clickerActive ? kAccent : kMuted);
    drawText(dc, clickerActive ? L"Кликер активен" : L"Кликер неактивен",
             {clickerPill.left + 28, 27, clickerPill.right - 10, 59}, smallFont_,
             clickerActive ? kAccent : kMuted);
}

void MainWindow::paintGallery(HDC dc, const RECT& client) {
    const int left = kSidebarWidth + 34;
    const int right = client.right - 28;
    drawText(dc, L"Галерея прицелов", {left, 28, right - 170, 66}, titleFont_, kText);
    drawText(dc, L"360 процедурных пресетов: статичные, пульсирующие и вращающиеся",
             {left, 68, right, 94}, bodyFont_, kMuted);

    struct FilterItem { Filter filter; const wchar_t* label; int width; };
    constexpr std::array<FilterItem, 6> filters{{
        {Filter::All, L"Все  360", 86},
        {Filter::Static, L"Статика", 86},
        {Filter::Animated, L"Анимация", 96},
        {Filter::Minimal, L"Минимализм", 112},
        {Filter::Tactical, L"Тактика", 86},
        {Filter::Dynamic, L"Динамика", 98},
    }};
    int filterX = left;
    for (const auto& item : filters) {
        const RECT bounds{filterX, 108, filterX + item.width, 144};
        const bool selected = filter_ == item.filter;
        fillRound(dc, bounds, 10, selected ? kAccentDark : kCard);
        if (selected) {
            strokeRound(dc, bounds, 10, kAccent);
        }
        drawText(dc, item.label, bounds, smallFont_, selected ? kAccent : kMuted,
                 DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        addHit(bounds, Action::SetFilter, static_cast<int>(item.filter));
        filterX += item.width + 9;
    }

    const int gridTop = 163;
    const int gridBottom = client.bottom - 22;
    const int availableWidth = right - left;
    const int columns = availableWidth >= 760 ? 4 : 3;
    const int gap = 13;
    const int cardWidth = (availableWidth - gap * (columns - 1)) / columns;
    const int cardHeight = 142;
    const int rowStride = cardHeight + gap;
    const auto indices = filteredPresets();
    const int rows = static_cast<int>((indices.size() + static_cast<std::size_t>(columns - 1)) /
                                      static_cast<std::size_t>(columns));
    galleryMaximumScroll_ = std::max(0, rows * rowStride - (gridBottom - gridTop));
    galleryScroll_ = clampValue(galleryScroll_, 0, galleryMaximumScroll_);

    const int saved = SaveDC(dc);
    IntersectClipRect(dc, left, gridTop, right + 1, gridBottom + 1);
    for (std::size_t position = 0; position < indices.size(); ++position) {
        const int row = static_cast<int>(position) / columns;
        const int column = static_cast<int>(position) % columns;
        const int y = gridTop + row * rowStride - galleryScroll_;
        if (y + cardHeight < gridTop || y > gridBottom) {
            continue;
        }
        const int x = left + column * (cardWidth + gap);
        const RECT card{x, y, x + cardWidth, y + cardHeight};
        const int presetIndex = indices[position];
        const auto& preset = presets_[static_cast<std::size_t>(presetIndex)];
        const bool selected = settings_.selectedPreset == presetIndex;
        fillRound(dc, card, 13, selected ? RGB(27, 47, 43) : kCard);
        strokeRound(dc, card, 13, selected ? kAccent : kBorder, selected ? 2 : 1);
        drawCrosshairPreview(dc, {x + cardWidth / 2, y + 46}, presetIndex, 0.62F);

        RECT nameRect{x + 13, y + 78, x + cardWidth - 13, y + 101};
        drawText(dc, preset.name, nameRect, smallFont_, kText, DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
        const bool animated = preset.animation != Animation::None;
        const RECT badge{x + 12, y + 109, x + (animated ? 86 : 74), y + 131};
        fillRound(dc, badge, 8, animated ? RGB(55, 43, 83) : RGB(34, 42, 54));
        drawText(dc, animated ? L"АНИМАЦИЯ" : L"СТАТИКА", badge, smallFont_,
                 animated ? RGB(193, 159, 255) : kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        if (selected) {
            fillRound(dc, {card.right - 30, card.top + 11, card.right - 12, card.top + 29}, 9, kAccent);
            drawText(dc, L"✓", {card.right - 30, card.top + 9, card.right - 12, card.top + 29},
                     smallFont_, RGB(12, 54, 40), DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        }
        const RECT clickable{card.left, std::max(card.top, static_cast<LONG>(gridTop)),
                             card.right, std::min(card.bottom, static_cast<LONG>(gridBottom))};
        addHit(clickable, Action::SelectPreset, presetIndex);
    }
    RestoreDC(dc, saved);

    if (galleryMaximumScroll_ > 0) {
        const int trackHeight = gridBottom - gridTop;
        const int thumbHeight = std::max(36, trackHeight * trackHeight / (trackHeight + galleryMaximumScroll_));
        const int thumbY = gridTop + (trackHeight - thumbHeight) * galleryScroll_ / galleryMaximumScroll_;
        fillRound(dc, {right + 7, gridTop, right + 11, gridBottom}, 4, RGB(29, 34, 43));
        fillRound(dc, {right + 7, thumbY, right + 11, thumbY + thumbHeight}, 4, kMuted);
    }
}

void MainWindow::paintCrosshair(HDC dc, const RECT& client) {
    const int left = kSidebarWidth + 34;
    const int right = client.right - 28;
    drawText(dc, L"Мой прицел", {left, 28, right - 170, 66}, titleFont_, kText);
    drawText(dc, L"Точная настройка выбранного пресета в реальном времени",
             {left, 68, right, 94}, bodyFont_, kMuted);

    const int contentTop = 116;
    const int previewWidth = clampValue((right - left) * 46 / 100, 325, 440);
    const RECT preview{left, contentTop, left + previewWidth, client.bottom - 30};
    fillRound(dc, preview, 16, kCard);
    strokeRound(dc, preview, 16, kBorder);

    const auto& selected = presets_[static_cast<std::size_t>(settings_.selectedPreset)];
    const int previewCenterY = preview.top + (preview.bottom - preview.top) * 43 / 100;
    fillRound(dc, {preview.left + 24, preview.top + 22, preview.right - 24, previewCenterY + 88}, 14,
              RGB(17, 21, 29));
    drawCrosshairPreview(dc, {(preview.left + preview.right) / 2, previewCenterY}, settings_.selectedPreset, 1.35F);
    drawText(dc, selected.name, {preview.left + 25, preview.bottom - 116, preview.right - 25, preview.bottom - 83},
             headingFont_, kText, DT_CENTER | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    drawText(dc, selected.family + L"  •  " + animationName(selected.animation),
             {preview.left + 25, preview.bottom - 84, preview.right - 25, preview.bottom - 58},
             smallFont_, kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);

    const int buttonY = preview.bottom - 48;
    const int compactWidth = (previewWidth - 68) / 3;
    drawButton(dc, RECT{preview.left + 18, buttonY, preview.left + 18 + compactWidth, buttonY + 34},
               L"←", Action::PreviousPreset);
    drawButton(dc, RECT{preview.left + 26 + compactWidth, buttonY, preview.left + 26 + compactWidth * 2, buttonY + 34},
               L"Случайный", Action::RandomPreset, 0, true);
    drawButton(dc, RECT{preview.left + 34 + compactWidth * 2, buttonY, preview.right - 18, buttonY + 34},
               L"→", Action::NextPreset);

    const int controlsX = preview.right + 25;
    const int controlsWidth = right - controlsX;
    drawText(dc, L"Внешний вид", {controlsX, contentTop, right, contentTop + 34}, headingFont_, kText);
    drawSlider(dc, controlsX, contentTop + 44, controlsWidth, L"Размер", settings_.sizePercent,
               50, 180, L"%", Action::SizeSlider);
    drawSlider(dc, controlsX, contentTop + 119, controlsWidth, L"Прозрачность", settings_.opacityPercent,
               20, 100, L"%", Action::OpacitySlider);
    drawSwitch(dc, controlsX, contentTop + 202, L"Тёмная обводка",
               L"Контраст на светлом фоне", settings_.forceOutline, Action::ToggleOutline);
    drawSwitch(dc, controlsX, contentTop + 274, L"Центральная точка",
               L"Добавить точку к любому пресету", settings_.forceCenterDot, Action::ToggleCenterDot);
    drawSwitch(dc, controlsX, contentTop + 346, L"Показывать оверлей",
               L"Горячая клавиша Ctrl + Shift + O", settings_.overlayVisible, Action::ToggleOverlay);
}

void MainWindow::paintClicker(HDC dc, const RECT& client) {
    const int left = kSidebarWidth + 34;
    const int right = client.right - 28;
    drawText(dc, L"Автокликер", {left, 28, right - 170, 66}, titleFont_, kText);
    drawText(dc, L"Системные клики через Windows SendInput — без доступа к памяти игр",
             {left, 68, right, 94}, bodyFont_, kMuted);

    const RECT status{left, 112, right, 202};
    fillRound(dc, status, 16, clicker_.isActive() ? RGB(23, 65, 51) : kCard);
    strokeRound(dc, status, 16, clicker_.isActive() ? kAccentDark : kBorder);
    fillRound(dc, {status.left + 22, status.top + 25, status.left + 62, status.top + 65}, 20,
              clicker_.isActive() ? kAccent : RGB(55, 63, 78));
    drawText(dc, clicker_.isActive() ? L"●" : L"○", {status.left + 22, status.top + 23, status.left + 62, status.top + 63},
             headingFont_, clicker_.isActive() ? RGB(12, 55, 41) : kMuted,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    drawText(dc, clicker_.isActive() ? L"Автокликер работает" : L"Автокликер готов",
             {status.left + 78, status.top + 14, status.right - 190, status.top + 48}, headingFont_, kText);
    const std::wstring activationHint = clicker_.isActive()
                                            ? L"Esc — аварийная остановка"
                                            : (clickerHotkeyRegistered_
                                                   ? L"Запуск: " + hotkeyName(settings_.clickerHotkey)
                                                   : L"Горячая клавиша занята другой программой");
    drawText(dc, activationHint,
             {status.left + 78, status.top + 48, status.right - 190, status.top + 75}, bodyFont_,
             clickerHotkeyRegistered_ ? kMuted : kDanger);

    const RECT master{status.right - 158, status.top + 26, status.right - 22, status.top + 64};
    fillRound(dc, master, 19, settings_.clickerEnabled ? kAccent : RGB(53, 61, 75));
    drawText(dc, settings_.clickerEnabled ? L"ВКЛЮЧЁН" : L"ВЫКЛЮЧЕН", master, smallFont_,
             settings_.clickerEnabled ? RGB(10, 55, 40) : kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    addHit(master, Action::ToggleClicker);

    const int gap = 16;
    const int panelWidth = (right - left - gap) / 2;
    const RECT timing{left, 220, left + panelWidth, client.bottom - 30};
    const RECT behavior{timing.right + gap, 220, right, client.bottom - 30};
    fillRound(dc, timing, 16, kCard);
    fillRound(dc, behavior, 16, kCard);
    strokeRound(dc, timing, 16, kBorder);
    strokeRound(dc, behavior, 16, kBorder);

    drawText(dc, L"Скорость и ритм", {timing.left + 22, timing.top + 15, timing.right - 22, timing.top + 48},
             headingFont_, kText);
    drawSlider(dc, timing.left + 22, timing.top + 57, panelWidth - 44, L"Кликов в секунду",
               settings_.clicksPerSecond, 1, 30, L" CPS", Action::CpsSlider);
    drawSlider(dc, timing.left + 22, timing.top + 137, panelWidth - 44, L"Разброс интервала",
               settings_.intervalVariationPercent, 0, 35, L"%", Action::VariationSlider);
    drawText(dc, L"Режим активации", {timing.left + 22, timing.top + 226, timing.right - 22, timing.top + 250},
             bodyFont_, kMuted);
    drawSegment(dc, {timing.left + 22, timing.top + 258, timing.right - 22, timing.top + 298},
                {L"Переключатель", L"Удержание"}, static_cast<int>(settings_.clickMode), Action::SetClickMode);
    drawText(dc, L"Кнопка мыши", {timing.left + 22, timing.top + 316, timing.right - 22, timing.top + 340},
             bodyFont_, kMuted);
    drawSegment(dc, {timing.left + 22, timing.top + 348, timing.right - 22, timing.top + 388},
                {L"Левая", L"Правая"}, static_cast<int>(settings_.clickButton), Action::SetClickButton);

    drawText(dc, L"Поведение", {behavior.left + 22, behavior.top + 15, behavior.right - 22, behavior.top + 48},
             headingFont_, kText);
    drawText(dc, L"Кликов в серии", {behavior.left + 22, behavior.top + 64, behavior.right - 22, behavior.top + 88},
             bodyFont_, kMuted);
    drawSegment(dc, {behavior.left + 22, behavior.top + 96, behavior.right - 22, behavior.top + 136},
                {L"1", L"2", L"3"}, settings_.burstCount - 1, Action::SetBurst, {1, 2, 3});
    drawText(dc, L"Горячая клавиша", {behavior.left + 22, behavior.top + 158, behavior.right - 22, behavior.top + 182},
             bodyFont_, kMuted);
    drawSegment(dc, {behavior.left + 22, behavior.top + 190, behavior.right - 22, behavior.top + 230},
                {L"F6", L"F7", L"F8", L"F9"}, settings_.clickerHotkey - VK_F6,
                Action::SetHotkey, {VK_F6, VK_F7, VK_F8, VK_F9});

    const RECT safety{behavior.left + 22, behavior.top + 255, behavior.right - 22, behavior.bottom - 20};
    fillRound(dc, safety, 12, RGB(38, 35, 29));
    drawText(dc, L"БЕЗОПАСНОСТЬ", {safety.left + 15, safety.top + 10, safety.right - 15, safety.top + 32},
             smallFont_, kWarning);
    RECT safetyText{safety.left + 15, safety.top + 34, safety.right - 15, safety.bottom - 10};
    drawText(dc, L"Нажмите Esc для мгновенной остановки. Проверьте правила игры: некоторые проекты запрещают автокликеры.",
             safetyText, smallFont_, kMuted, DT_LEFT | DT_TOP | DT_WORDBREAK);
}

void MainWindow::paintSettings(HDC dc, const RECT& client) {
    const int left = kSidebarWidth + 34;
    const int right = client.right - 28;
    drawText(dc, L"Настройки", {left, 28, right - 170, 66}, titleFont_, kText);
    drawText(dc, L"Оверлей, положение и поведение приложения",
             {left, 68, right, 94}, bodyFont_, kMuted);

    const int gap = 16;
    const int panelWidth = (right - left - gap) / 2;
    const RECT overlayPanel{left, 116, left + panelWidth, client.bottom - 30};
    const RECT appPanel{overlayPanel.right + gap, 116, right, client.bottom - 30};
    fillRound(dc, overlayPanel, 16, kCard);
    fillRound(dc, appPanel, 16, kCard);
    strokeRound(dc, overlayPanel, 16, kBorder);
    strokeRound(dc, appPanel, 16, kBorder);

    drawText(dc, L"Оверлей", {overlayPanel.left + 22, overlayPanel.top + 15, overlayPanel.right - 22, overlayPanel.top + 48},
             headingFont_, kText);
    drawSwitch(dc, overlayPanel.left + 22, overlayPanel.top + 56, L"Показывать прицел",
               L"Работает поверх окон без перехвата мыши", settings_.overlayVisible, Action::ToggleOverlay);
    drawSwitch(dc, overlayPanel.left + 22, overlayPanel.top + 128, L"Активный монитор",
               L"Следовать за текущей игрой или окном", settings_.followActiveMonitor, Action::ToggleActiveMonitor);
    drawSlider(dc, overlayPanel.left + 22, overlayPanel.top + 218, panelWidth - 44, L"Смещение по X",
               settings_.offsetX, -100, 100, L" px", Action::OffsetXSlider);
    drawSlider(dc, overlayPanel.left + 22, overlayPanel.top + 298, panelWidth - 44, L"Смещение по Y",
               settings_.offsetY, -100, 100, L" px", Action::OffsetYSlider);
    const RECT hint{overlayPanel.left + 22, overlayPanel.top + 392, overlayPanel.right - 22, overlayPanel.bottom - 22};
    fillRound(dc, hint, 12, RGB(21, 38, 36));
    drawText(dc, L"Ctrl + Shift + O", {hint.left + 15, hint.top + 10, hint.right - 15, hint.top + 34},
             bodyFont_, kAccent);
    RECT hintBody{hint.left + 15, hint.top + 38, hint.right - 15, hint.bottom - 10};
    drawText(dc, L"Мгновенно скрывает или возвращает прицел поверх игры.", hintBody, smallFont_, kMuted,
             DT_LEFT | DT_TOP | DT_WORDBREAK);

    drawText(dc, L"Приложение", {appPanel.left + 22, appPanel.top + 15, appPanel.right - 22, appPanel.top + 48},
             headingFont_, kText);
    drawSwitch(dc, appPanel.left + 22, appPanel.top + 56, L"Сворачивать в трей",
               L"Крестик не завершает AimPoint", settings_.minimizeToTray, Action::ToggleTray);
    drawSwitch(dc, appPanel.left + 22, appPanel.top + 128, L"Запускать с Windows",
               L"Стартовать свёрнутым в области уведомлений", settings_.runAtStartup, Action::ToggleStartup);

    const RECT about{appPanel.left + 22, appPanel.top + 218, appPanel.right - 22, appPanel.top + 348};
    fillRound(dc, about, 12, RGB(20, 24, 33));
    drawText(dc, L"AimPoint 1.0", {about.left + 16, about.top + 12, about.right - 16, about.top + 42},
             headingFont_, kText);
    RECT aboutBody{about.left + 16, about.top + 48, about.right - 16, about.bottom - 12};
    drawText(dc, L"Нативное C++/Win32-приложение. Оверлей не внедряется в процессы и не читает память игр.",
             aboutBody, smallFont_, kMuted, DT_LEFT | DT_TOP | DT_WORDBREAK);

    drawButton(dc, {appPanel.left + 22, appPanel.bottom - 60, appPanel.right - 22, appPanel.bottom - 20},
               L"Завершить AimPoint", Action::ExitApplication);
}

void MainWindow::addHit(const RECT bounds, const Action action, const int value) {
    HitRegion region;
    region.bounds = bounds;
    region.action = action;
    region.value = value;
    hits_.push_back(region);
}

void MainWindow::drawButton(HDC dc,
                            const RECT bounds,
                            const wchar_t* label,
                            const Action action,
                            const int value,
                            const bool accent) {
    fillRound(dc, bounds, 10, accent ? kAccentDark : RGB(34, 40, 52));
    strokeRound(dc, bounds, 10, accent ? kAccent : kBorder);
    drawText(dc, label, bounds, bodyFont_, accent ? kAccent : kText,
             DT_CENTER | DT_VCENTER | DT_SINGLELINE);
    addHit(bounds, action, value);
}

void MainWindow::drawSwitch(HDC dc,
                            const int x,
                            const int y,
                            const wchar_t* title,
                            const wchar_t* subtitle,
                            const bool enabled,
                            const Action action) {
    const RECT bounds{x, y, x + 320, y + 64};
    drawText(dc, title, {x, y, x + 243, y + 29}, bodyFont_, kText);
    drawText(dc, subtitle, {x, y + 27, x + 250, y + 54}, smallFont_, kMuted,
             DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_END_ELLIPSIS);
    const RECT track{x + 258, y + 10, x + 310, y + 38};
    fillRound(dc, track, 16, enabled ? kAccent : RGB(57, 64, 78));
    const int knobLeft = enabled ? track.right - 25 : track.left + 3;
    fillRound(dc, {knobLeft, track.top + 3, knobLeft + 22, track.bottom - 3}, 12,
              enabled ? RGB(15, 76, 56) : RGB(179, 187, 200));
    addHit(bounds, action);
}

void MainWindow::drawSlider(HDC dc,
                            const int x,
                            const int y,
                            const int width,
                            const wchar_t* label,
                            const int value,
                            const int minimum,
                            const int maximum,
                            const wchar_t* suffix,
                            const Action action) {
    drawText(dc, label, {x, y, x + width - 90, y + 27}, bodyFont_, kMuted);
    drawText(dc, std::to_wstring(value) + suffix, {x + width - 90, y, x + width, y + 27},
             bodyFont_, kText, DT_RIGHT | DT_VCENTER | DT_SINGLELINE);
    const int trackY = y + 42;
    const RECT track{x, trackY, x + width, trackY + 6};
    fillRound(dc, track, 6, RGB(50, 57, 71));
    const float ratio = static_cast<float>(value - minimum) / static_cast<float>(maximum - minimum);
    const int knobX = x + static_cast<int>(std::lround(ratio * static_cast<float>(width)));
    fillRound(dc, {x, trackY, knobX, trackY + 6}, 6, kAccent);
    fillRound(dc, {knobX - 7, trackY - 5, knobX + 7, trackY + 11}, 9, kAccent);
    strokeRound(dc, {knobX - 7, trackY - 5, knobX + 7, trackY + 11}, 9, RGB(20, 85, 63));
    addHit({x - 5, trackY - 12, x + width + 5, trackY + 19}, action);
}

void MainWindow::drawSegment(HDC dc,
                             const RECT bounds,
                             const std::vector<const wchar_t*>& labels,
                             const int selected,
                             const Action action,
                             const std::vector<int>& values) {
    fillRound(dc, bounds, 10, RGB(18, 22, 30));
    const int count = static_cast<int>(labels.size());
    const int width = (bounds.right - bounds.left) / count;
    for (int index = 0; index < count; ++index) {
        RECT item{bounds.left + index * width, bounds.top,
                  index + 1 == count ? bounds.right : bounds.left + (index + 1) * width,
                  bounds.bottom};
        if (index == selected) {
            fillRound(dc, {item.left + 3, item.top + 3, item.right - 3, item.bottom - 3}, 8, kAccentDark);
        }
        drawText(dc, labels[static_cast<std::size_t>(index)], item, smallFont_,
                 index == selected ? kAccent : kMuted, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        const int value = values.empty() ? index : values[static_cast<std::size_t>(index)];
        addHit(item, action, value);
    }
}

void MainWindow::drawCrosshairPreview(HDC dc, const POINT center, const int presetIndex, const float scale) {
    RenderOptions options;
    options.scale = scale * static_cast<float>(settings_.sizePercent) / 100.0F;
    options.outline = settings_.forceOutline;
    options.centerDot = settings_.forceCenterDot;
    options.elapsedMilliseconds = GetTickCount64() - startedAt_;
    CrosshairRenderer::draw(dc, center, presets_[static_cast<std::size_t>(presetIndex)], options);
}

std::vector<int> MainWindow::filteredPresets() const {
    std::vector<int> result;
    result.reserve(presets_.size());
    for (std::size_t index = 0; index < presets_.size(); ++index) {
        if (filterMatches(presets_[index])) {
            result.push_back(static_cast<int>(index));
        }
    }
    return result;
}

bool MainWindow::filterMatches(const CrosshairPreset& preset) const {
    switch (filter_) {
    case Filter::All:
        return true;
    case Filter::Static:
        return preset.animation == Animation::None;
    case Filter::Animated:
        return preset.animation != Animation::None;
    case Filter::Minimal:
        return isMinimalShape(preset.shape);
    case Filter::Tactical:
        return preset.family == L"Тактические";
    case Filter::Dynamic:
        return preset.family == L"Динамические";
    }
    return true;
}

void MainWindow::processAction(const Action action, const int value, const POINT point) {
    bool changed = false;
    bool hotkeyChanged = false;
    switch (action) {
    case Action::None:
        return;
    case Action::Navigate:
        page_ = static_cast<Page>(value);
        InvalidateRect(window_, nullptr, FALSE);
        return;
    case Action::SelectPreset:
        settings_.selectedPreset = clampValue(value, 0, static_cast<int>(presets_.size()) - 1);
        changed = true;
        break;
    case Action::SetFilter:
        filter_ = static_cast<Filter>(value);
        galleryScroll_ = 0;
        InvalidateRect(window_, nullptr, FALSE);
        return;
    case Action::SizeSlider:
    case Action::OpacitySlider:
    case Action::OffsetXSlider:
    case Action::OffsetYSlider:
    case Action::CpsSlider:
    case Action::VariationSlider:
        updateSlider(action, point.x);
        return;
    case Action::ToggleOverlay:
        settings_.overlayVisible = !settings_.overlayVisible;
        changed = true;
        break;
    case Action::ToggleOutline:
        settings_.forceOutline = !settings_.forceOutline;
        changed = true;
        break;
    case Action::ToggleCenterDot:
        settings_.forceCenterDot = !settings_.forceCenterDot;
        changed = true;
        break;
    case Action::ToggleActiveMonitor:
        settings_.followActiveMonitor = !settings_.followActiveMonitor;
        changed = true;
        break;
    case Action::PreviousPreset:
        settings_.selectedPreset = (settings_.selectedPreset + static_cast<int>(presets_.size()) - 1) %
                                   static_cast<int>(presets_.size());
        changed = true;
        break;
    case Action::RandomPreset: {
        static std::mt19937 random(std::random_device{}());
        std::uniform_int_distribution<int> distribution(0, static_cast<int>(presets_.size()) - 1);
        settings_.selectedPreset = distribution(random);
        changed = true;
        break;
    }
    case Action::NextPreset:
        settings_.selectedPreset = (settings_.selectedPreset + 1) % static_cast<int>(presets_.size());
        changed = true;
        break;
    case Action::ToggleClicker:
        settings_.clickerEnabled = !settings_.clickerEnabled;
        if (!settings_.clickerEnabled) {
            clicker_.panicStop();
        }
        changed = true;
        break;
    case Action::SetClickMode:
        settings_.clickMode = value == 1 ? ClickMode::Hold : ClickMode::Toggle;
        clicker_.panicStop();
        changed = true;
        break;
    case Action::SetClickButton:
        settings_.clickButton = value == 1 ? ClickButton::Right : ClickButton::Left;
        changed = true;
        break;
    case Action::SetBurst:
        settings_.burstCount = clampValue(value, 1, 3);
        changed = true;
        break;
    case Action::SetHotkey:
        settings_.clickerHotkey = clampValue(value, static_cast<int>(VK_F6), static_cast<int>(VK_F9));
        clicker_.panicStop();
        changed = true;
        hotkeyChanged = true;
        break;
    case Action::ToggleTray:
        settings_.minimizeToTray = !settings_.minimizeToTray;
        changed = true;
        break;
    case Action::ToggleStartup: {
        const bool desired = !settings_.runAtStartup;
        if (store_.setRunAtStartup(desired)) {
            settings_.runAtStartup = desired;
            changed = true;
        } else {
            MessageBoxW(window_, L"Не удалось изменить автозапуск. Проверьте права текущего пользователя.",
                        L"AimPoint", MB_OK | MB_ICONWARNING);
        }
        break;
    }
    case Action::ExitApplication:
        closingForExit_ = true;
        DestroyWindow(window_);
        return;
    }
    if (changed) {
        applySettings(hotkeyChanged);
    }
}

void MainWindow::updateSlider(const Action action, const int mouseX) {
    const int width = std::max(1, static_cast<int>(draggingBounds_.right - draggingBounds_.left) - 10);
    const int start = draggingBounds_.left + 5;
    const float ratio = clampValue(static_cast<float>(mouseX - start) / static_cast<float>(width), 0.0F, 1.0F);
    const auto interpolate = [ratio](const int minimum, const int maximum) {
        return minimum + static_cast<int>(std::lround(ratio * static_cast<float>(maximum - minimum)));
    };

    switch (action) {
    case Action::SizeSlider:
        settings_.sizePercent = interpolate(50, 180);
        break;
    case Action::OpacitySlider:
        settings_.opacityPercent = interpolate(20, 100);
        break;
    case Action::OffsetXSlider:
        settings_.offsetX = interpolate(-100, 100);
        break;
    case Action::OffsetYSlider:
        settings_.offsetY = interpolate(-100, 100);
        break;
    case Action::CpsSlider:
        settings_.clicksPerSecond = interpolate(1, 30);
        break;
    case Action::VariationSlider:
        settings_.intervalVariationPercent = interpolate(0, 35);
        break;
    default:
        return;
    }
    clicker_.configure(settings_);
    overlay_.update(settings_, presets_.at(static_cast<std::size_t>(settings_.selectedPreset)),
                    GetTickCount64() - startedAt_);
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::applySettings(const bool hotkeyChanged) {
    settings_.selectedPreset = clampValue(settings_.selectedPreset, 0, static_cast<int>(presets_.size()) - 1);
    clicker_.configure(settings_);
    if (hotkeyChanged) {
        registerGlobalHotkeys();
    }
    overlay_.update(settings_, presets_.at(static_cast<std::size_t>(settings_.selectedPreset)),
                    GetTickCount64() - startedAt_);
    store_.save(settings_);
    InvalidateRect(window_, nullptr, FALSE);
}

void MainWindow::registerGlobalHotkeys() {
    unregisterGlobalHotkeys();
    clickerHotkeyRegistered_ = RegisterHotKey(window_, kClickerHotkeyId, MOD_NOREPEAT,
                                               static_cast<UINT>(settings_.clickerHotkey)) != FALSE;
    RegisterHotKey(window_, kOverlayHotkeyId, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 'O');
}

void MainWindow::unregisterGlobalHotkeys() {
    if (!window_) {
        return;
    }
    UnregisterHotKey(window_, kClickerHotkeyId);
    UnregisterHotKey(window_, kOverlayHotkeyId);
    clickerHotkeyRegistered_ = false;
}

void MainWindow::showMainWindow() {
    ShowWindow(window_, SW_SHOW);
    ShowWindow(window_, SW_RESTORE);
    SetForegroundWindow(window_);
}

void MainWindow::addTrayIcon() {
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window_;
    icon.uID = 1;
    icon.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP;
    icon.uCallbackMessage = kTrayMessage;
    icon.hIcon = LoadIconW(instance_, MAKEINTRESOURCEW(IDI_AIMPOINT));
    lstrcpynW(icon.szTip, L"AimPoint — прицелы и автокликер",
              static_cast<int>(sizeof(icon.szTip) / sizeof(icon.szTip[0])));
    trayAdded_ = Shell_NotifyIconW(NIM_ADD, &icon) != FALSE;
    if (trayAdded_) {
        icon.uVersion = NOTIFYICON_VERSION_4;
        Shell_NotifyIconW(NIM_SETVERSION, &icon);
    }
}

void MainWindow::removeTrayIcon() {
    if (!trayAdded_) {
        return;
    }
    NOTIFYICONDATAW icon{};
    icon.cbSize = sizeof(icon);
    icon.hWnd = window_;
    icon.uID = 1;
    Shell_NotifyIconW(NIM_DELETE, &icon);
    trayAdded_ = false;
}

void MainWindow::showTrayMenu() {
    HMENU menu = CreatePopupMenu();
    AppendMenuW(menu, MF_STRING, kTrayShow, L"Открыть AimPoint");
    AppendMenuW(menu, MF_STRING | (settings_.overlayVisible ? MF_CHECKED : 0),
                kTrayOverlay, L"Показывать прицел");
    AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
    AppendMenuW(menu, MF_STRING, kTrayExit, L"Выход");
    POINT cursor{};
    GetCursorPos(&cursor);
    SetForegroundWindow(window_);
    TrackPopupMenu(menu, TPM_RIGHTBUTTON | TPM_BOTTOMALIGN, cursor.x, cursor.y, 0, window_, nullptr);
    DestroyMenu(menu);
}

void MainWindow::destroyFonts() {
    for (HFONT* font : {&logoFont_, &titleFont_, &headingFont_, &bodyFont_, &smallFont_}) {
        if (*font) {
            DeleteObject(*font);
            *font = nullptr;
        }
    }
}

} // namespace aimpoint
