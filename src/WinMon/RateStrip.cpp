#include "RateStrip.h"
#include "Theme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cwchar>
#include <iterator>

namespace
{
constexpr wchar_t kTaskbarClassName[] = L"Shell_TrayWnd";
constexpr wchar_t kNotificationAreaClassName[] = L"TrayNotifyWnd";
constexpr wchar_t kMaximumTopLine[] = L"999.9 G/s \u2191";
constexpr wchar_t kMaximumBottomLine[] = L"999.9 G/s \u2193";
struct NotificationAreaSearch
{
    HWND notificationArea = nullptr;
};

struct FloatingVisibilitySearch
{
    RECT target{};
    bool visible = false;
};

BOOL CALLBACK FindNotificationAreaChild(HWND child, LPARAM parameter)
{
    auto* const search = reinterpret_cast<NotificationAreaSearch*>(parameter);

    wchar_t className[64]{};
    if (GetClassNameW(child, className, static_cast<int>(std::size(className))) > 0 &&
        wcscmp(className, kNotificationAreaClassName) == 0)
    {
        search->notificationArea = child;
        return FALSE;
    }

    return TRUE;
}

BOOL CALLBACK FindVisibleFloatingArea(HMONITOR monitor, HDC, LPRECT, LPARAM parameter)
{
    auto* const search = reinterpret_cast<FloatingVisibilitySearch*>(parameter);
    MONITORINFO monitorInfo{ sizeof(monitorInfo) };
    if (!GetMonitorInfoW(monitor, &monitorInfo))
    {
        return TRUE;
    }

    if (FloatingRateRenderer::IsPositionVisible(search->target, monitorInfo.rcWork))
    {
        search->visible = true;
        return FALSE;
    }
    return TRUE;
}

bool IsFloatingPositionVisible(POINT position, CSize size, UINT dpi) noexcept
{
    const int margin = MulDiv(3, static_cast<int>(dpi), 96);
    FloatingVisibilitySearch search{
        {position.x + margin, position.y + margin, position.x + size.cx - margin, position.y + size.cy - margin}};
    EnumDisplayMonitors(nullptr, nullptr, FindVisibleFloatingArea, reinterpret_cast<LPARAM>(&search));
    return search.visible;
}

POINT DefaultFloatingPosition(CSize size) noexcept
{
    MONITORINFO monitorInfo{ sizeof(monitorInfo) };
    const HMONITOR monitor = MonitorFromPoint({}, MONITOR_DEFAULTTOPRIMARY);
    if (monitor == nullptr || !GetMonitorInfoW(monitor, &monitorInfo))
    {
        return {};
    }

    const int margin = MulDiv(12, static_cast<int>(GetDpiForSystem()), 96);
    return {
        std::max(monitorInfo.rcWork.left, monitorInfo.rcWork.right - size.cx - margin),
        monitorInfo.rcWork.top + margin};
}
}
void RateStrip::SetRates(
    const std::wstring& uploadText,
    const std::wstring& downloadText,
    double uploadBytesPerSecond,
    double downloadBytesPerSecond) noexcept
{
    uploadText_ = uploadText;
    downloadText_ = downloadText;
    floatingRenderer_.AddSample(uploadBytesPerSecond, downloadBytesPerSecond);
    UpdatePulseTimer();
    if (GetSafeHwnd() != nullptr)
    {
        static_cast<void>(Render());
    }
}


std::wstring RateStrip::GetRateFontName() const
{
    RateFontSelection selection;
    return GetRateFont(selection) ? selection.logFont.lfFaceName : L"";
}

bool RateStrip::GetRateFont(RateFontSelection& selection) const noexcept
{
    if (hasSelectedFont_)
    {
        selection = selectedFont_;
        return true;
    }

    const HWND taskbar = taskbar_ != nullptr && IsWindow(taskbar_) ? taskbar_ : nullptr;
    return LoadDefaultRateFont(taskbar, selection);
}

bool RateStrip::SetRateFont(const RateFontSelection& selection) noexcept
{
    if (selection.logFont.lfFaceName[0] == L'\0' || selection.pointSizeTenths <= 0)
    {
        return false;
    }

    if (hasSelectedFont_ &&
        selectedFont_.pointSizeTenths == selection.pointSizeTenths &&
        memcmp(&selectedFont_.logFont, &selection.logFont, sizeof(LOGFONTW)) == 0)
    {
        return true;
    }

    const RateFontSelection previousFont = selectedFont_;
    const bool previouslySelected = hasSelectedFont_;
    selectedFont_ = selection;
    hasSelectedFont_ = true;

    if (GetSafeHwnd() == nullptr)
    {
        return true;
    }

    if (floating_ && RelayoutFloating() && Render())
    {
        return true;
    }

    const HWND taskbar = FindPrimaryBottomTaskbar();
    const HWND notificationArea = taskbar == nullptr ? nullptr : FindNotificationArea(taskbar);
    if (notificationArea != nullptr && taskbar_ == taskbar && Relayout(taskbar, notificationArea) && Render())
    {
        return true;
    }

    selectedFont_ = previousFont;
    hasSelectedFont_ = previouslySelected;
    if (floating_)
    {
        static_cast<void>(RelayoutFloating());
        static_cast<void>(Render());
    }
    else if (notificationArea != nullptr && taskbar_ == taskbar)
    {
        static_cast<void>(Relayout(taskbar, notificationArea));
        static_cast<void>(Render());
    }
    return false;
}

bool RateStrip::ResetRateFont() noexcept
{
    if (!hasSelectedFont_)
    {
        return true;
    }

    const RateFontSelection previousFont = selectedFont_;
    hasSelectedFont_ = false;
    if (GetSafeHwnd() == nullptr)
    {
        return true;
    }

    if (floating_ && RelayoutFloating() && Render())
    {
        return true;
    }

    const HWND taskbar = FindPrimaryBottomTaskbar();
    const HWND notificationArea = taskbar == nullptr ? nullptr : FindNotificationArea(taskbar);
    if (notificationArea != nullptr && taskbar_ == taskbar && Relayout(taskbar, notificationArea) && Render())
    {
        return true;
    }

    selectedFont_ = previousFont;
    hasSelectedFont_ = true;
    if (floating_)
    {
        static_cast<void>(RelayoutFloating());
        static_cast<void>(Render());
    }
    else if (notificationArea != nullptr && taskbar_ == taskbar)
    {
        static_cast<void>(Relayout(taskbar, notificationArea));
        static_cast<void>(Render());
    }
    return false;
}

void RateStrip::SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept
{
    contextMenuOwner_ = owner;
    contextMenuMessage_ = notificationMessage;
}

void RateStrip::SetPositionChangedOwner(HWND owner, UINT notificationMessage) noexcept
{
    positionChangedOwner_ = owner;
    positionChangedMessage_ = notificationMessage;
}

void RateStrip::SetContextMenuEnabled(bool enabled) noexcept
{
    if (contextMenuEnabled_ == enabled)
    {
        return;
    }

    contextMenuEnabled_ = enabled;
    // The hit-testable alpha floor changes with the setting, so repaint.
    if (GetSafeHwnd() != nullptr)
    {
        static_cast<void>(Render());
    }
}

BEGIN_MESSAGE_MAP(RateStrip, CWnd)
    ON_WM_PAINT()
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_TIMER()
    ON_WM_SETTINGCHANGE()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEACTIVATE()
    ON_WM_NCHITTEST()
    ON_WM_RBUTTONUP()
    ON_WM_LBUTTONDOWN()
    ON_WM_EXITSIZEMOVE()
    ON_WM_WINDOWPOSCHANGED()
    ON_MESSAGE(WM_DPICHANGED, &RateStrip::OnDpiChanged)
    ON_WM_DISPLAYCHANGE()
END_MESSAGE_MAP()

bool RateStrip::Embed(bool shouldShow)
{
    if (!shouldShow)
    {
        Shutdown();
        return false;
    }
    if (floating_)
    {
        Shutdown();
    }
    const HWND taskbar = FindPrimaryBottomTaskbar();
    if (taskbar == nullptr)
    {
        Shutdown();
        return false;
    }

    const HWND notificationArea = FindNotificationArea(taskbar);
    if (notificationArea == nullptr)
    {
        Shutdown();
        return false;
    }

    if (GetSafeHwnd() != nullptr)
    {
        if (taskbar_ != taskbar || !IsWindow(taskbar_))
        {
            Shutdown();
        }
        else if (!Relayout(taskbar, notificationArea))
        {
            Shutdown();
        }
        else
        {
            ShowWindow(SW_SHOWNOACTIVATE);
            return true;
        }
    }

    if (!CreateRateFont(taskbar))
    {
        return false;
    }

    naturalSize_ = MeasureSize(taskbar);
    size_ = naturalSize_;
    if (size_.cx <= 0 || size_.cy <= 0)
    {
        font_.DeleteObject();
        return false;
    }

    const CString windowClass = AfxRegisterWndClass(0, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr);
    const DWORD style = WS_POPUP | WS_SYSMENU;
    if (!CreateEx(
            WS_EX_TOOLWINDOW | WS_EX_LAYERED,
            windowClass,
            nullptr,
            style,
            CRect(0, 0, size_.cx, size_.cy),
            nullptr,
            0))
    {
        font_.DeleteObject();
        return false;
    }

    if (::SetParent(GetSafeHwnd(), taskbar) == nullptr)
    {
        Shutdown();
        return false;
    }

    taskbar_ = taskbar;
    if (!Relayout(taskbar, notificationArea))
    {
        Shutdown();
        return false;
    }

    ShowWindow(SW_SHOWNOACTIVATE);
    return Render();
}

bool RateStrip::ShowFloating(POINT position, bool restorePosition)
{
    if (GetSafeHwnd() != nullptr)
    {
        if (!floating_)
        {
            Shutdown();
        }
        else
        {
            ShowWindow(SW_SHOWNOACTIVATE);
            return true;
        }
    }

    size_ = FloatingRateRenderer::SizeForDpi(GetDpiForSystem());
    if (!restorePosition) position = DefaultFloatingPosition(size_);

    const CString windowClass = AfxRegisterWndClass(0, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr);
    // A separate WS_EX_TRANSPARENT layered window makes shadow pixels pass
    // clicks to other processes; HTTRANSPARENT alone only reaches this thread.
    if (!floatingShadow_.CreateEx(
            WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_NOACTIVATE,
            windowClass, nullptr, WS_POPUP,
            CRect(position.x, position.y, position.x + size_.cx, position.y + size_.cy), nullptr, 0))
        return false;
    if (!CreateEx(
            WS_EX_TOOLWINDOW | WS_EX_LAYERED | WS_EX_NOACTIVATE,
            windowClass,
            nullptr,
            WS_POPUP,
            CRect(position.x, position.y, position.x + size_.cx, position.y + size_.cy),
            &floatingShadow_,
            0))
    {
        floatingShadow_.DestroyWindow();
        return false;
    }

    floating_ = true;
    if (!RelayoutFloating())
    {
        Shutdown();
        return false;
    }
    position = GetFloatingPosition();
    if (::SetWindowPos(
            GetSafeHwnd(), HWND_TOPMOST, position.x, position.y, size_.cx, size_.cy, SWP_NOACTIVATE) == FALSE)
    {
        Shutdown();
        return false;
    }
    if (!Render())
    {
        Shutdown();
        return false;
    }
    floatingShadow_.ShowWindow(SW_SHOWNOACTIVATE);
    ShowWindow(SW_SHOWNOACTIVATE);
    return true;
}
bool RateStrip::Refresh()
{
    if (GetSafeHwnd() == nullptr)
    {
        return false;
    }
    if (floating_)
    {
        return Render();
    }

    const HWND taskbar = FindPrimaryBottomTaskbar();
    if (taskbar == nullptr)
    {
        Shutdown();
        return false;
    }

    if (taskbar_ != taskbar || !IsWindow(taskbar_))
    {
        Shutdown();
        return false;
    }

    const HWND notificationArea = FindNotificationArea(taskbar);
    if (notificationArea == nullptr)
    {
        // Windows 11 may temporarily make TrayNotifyWnd unavailable in the
        // taskbar child tree while Start is open. Keep the existing strip at
        // its last valid position; SetRates renders and the next sample retries.
        return true;
    }

    ShowWindow(SW_HIDE);
    const COLORREF previousTextColor = textColor_;
    if (!PlaceBesideNotificationArea(taskbar, notificationArea))
    {
        Shutdown();
        return false;
    }

    if (previousTextColor != textColor_ && !Render())
    {
        Shutdown();
        return false;
    }

    ShowWindow(SW_SHOWNOACTIVATE);
    return true;
}

bool RateStrip::Relayout(HWND taskbar, HWND notificationArea) noexcept
{
    if (font_.GetSafeHandle() != nullptr)
    {
        font_.DeleteObject();
    }
    if (!CreateRateFont(taskbar))
    {
        return false;
    }
    naturalSize_ = MeasureSize(taskbar);
    size_ = naturalSize_;
    if (size_.cx <= 0 || size_.cy <= 0)
    {
        return false;
    }
    return PlaceBesideNotificationArea(taskbar, notificationArea);
}

bool RateStrip::RelayoutFloating() noexcept
{
    RECT windowRect{};
    if (GetSafeHwnd() == nullptr || !::GetWindowRect(GetSafeHwnd(), &windowRect))
    {
        return false;
    }
    UINT dpi = GetDpiForWindow(GetSafeHwnd());
    size_ = FloatingRateRenderer::SizeForDpi(dpi);
    POINT position{windowRect.left, windowRect.top};
    if (!IsFloatingPositionVisible(position, size_, dpi))
        position = DefaultFloatingPosition(size_);
    if (!::SetWindowPos(GetSafeHwnd(), HWND_TOPMOST, position.x, position.y, 0, 0,
            SWP_NOACTIVATE | SWP_NOSIZE)) return false;
    size_ = FloatingRateRenderer::SizeForDpi(GetDpiForWindow(GetSafeHwnd()));
    return true;
}

void RateStrip::Shutdown() noexcept
{
    if (GetSafeHwnd() && pulseTimerActive_) KillTimer(1);
    pulseTimerActive_ = false;
    pulsePhase_ = 0;
    hovering_ = false;
    textOpacity_ = 0;
    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }

    if (font_.GetSafeHandle() != nullptr)
    {
        font_.DeleteObject();
    }


    if (floatingShadow_.GetSafeHwnd()) floatingShadow_.DestroyWindow();
    floatingRenderer_.ReleaseTarget();
    taskbar_ = nullptr;
    size_ = {};
    floating_ = false;
}

bool RateStrip::IsPrimaryBottomTaskbarAvailable() noexcept
{
    return FindPrimaryBottomTaskbar() != nullptr;
}



HWND RateStrip::FindPrimaryBottomTaskbar() noexcept
{
    const HWND taskbar = ::FindWindowW(kTaskbarClassName, nullptr);
    if (taskbar == nullptr)
    {
        return nullptr;
    }

    const HMONITOR monitor = MonitorFromWindow(taskbar, MONITOR_DEFAULTTONULL);
    MONITORINFO monitorInfo{ sizeof(monitorInfo) };
    RECT taskbarRect{};
    if (monitor == nullptr ||
        !GetMonitorInfoW(monitor, &monitorInfo) ||
        (monitorInfo.dwFlags & MONITORINFOF_PRIMARY) == 0 ||
        !::GetWindowRect(taskbar, &taskbarRect) ||
        taskbarRect.bottom != monitorInfo.rcMonitor.bottom)
    {
        return nullptr;
    }

    return taskbar;
}

HWND RateStrip::FindNotificationArea(HWND taskbar) noexcept
{
    NotificationAreaSearch search;
    EnumChildWindows(taskbar, FindNotificationAreaChild, reinterpret_cast<LPARAM>(&search));
    return search.notificationArea;
}

bool RateStrip::LoadDefaultRateFont(HWND taskbar, RateFontSelection& selection) noexcept
{
    const UINT dpi = taskbar == nullptr ? GetDpiForSystem() : GetDpiForWindow(taskbar);
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoForDpi(
            SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi))
    {
        return false;
    }

    selection.logFont = metrics.lfMessageFont;
    const int logicalHeight = selection.logFont.lfHeight < 0
        ? -selection.logFont.lfHeight
        : selection.logFont.lfHeight;
    selection.pointSizeTenths = MulDiv(logicalHeight, 720, static_cast<int>(dpi));
    if (selection.logFont.lfFaceName[0] == L'\0' || selection.pointSizeTenths <= 0)
    {
        return false;
    }
    return true;
}

bool RateStrip::CreateRateFont(HWND taskbar) noexcept
{
    RateFontSelection selection;
    if (hasSelectedFont_)
    {
        selection = selectedFont_;
    }
    else if (!LoadDefaultRateFont(taskbar, selection))
    {
        return false;
    }

    const UINT dpi = taskbar == nullptr ? GetDpiForSystem() : GetDpiForWindow(taskbar);
    const int pointSizeTenths = taskbar == nullptr
        ? std::clamp(selection.pointSizeTenths, 80, 90)
        : selection.pointSizeTenths;
    selection.logFont.lfHeight = -MulDiv(pointSizeTenths, static_cast<int>(dpi), 720);
    return font_.CreateFontIndirectW(&selection.logFont) != FALSE;
}

CSize RateStrip::MeasureSize(HWND taskbar) const
{
    const HDC deviceContext = ::GetDC(taskbar);
    if (deviceContext == nullptr)
    {
        return {};
    }

    if (font_.GetSafeHandle() == nullptr)
    {
        ::ReleaseDC(taskbar, deviceContext);
        return {};
    }

    const HGDIOBJ previousFont = SelectObject(deviceContext, font_.GetSafeHandle());
    if (previousFont == nullptr)
    {
        ::ReleaseDC(taskbar, deviceContext);
        return {};
    }

    SIZE topSize{};
    SIZE bottomSize{};
    TEXTMETRICW textMetrics{};
    const bool measured =
        GetTextExtentPoint32W(deviceContext, kMaximumTopLine, static_cast<int>(std::size(kMaximumTopLine) - 1), &topSize) != FALSE &&
        GetTextExtentPoint32W(deviceContext, kMaximumBottomLine, static_cast<int>(std::size(kMaximumBottomLine) - 1), &bottomSize) != FALSE &&
        GetTextMetricsW(deviceContext, &textMetrics) != FALSE;

    SelectObject(deviceContext, previousFont);
    ::ReleaseDC(taskbar, deviceContext);

    if (!measured)
    {
        return {};
    }

    const UINT dpi = GetDpiForWindow(taskbar);
    const int horizontalPadding = MulDiv(8, static_cast<int>(dpi), 96);
    const int verticalPadding = MulDiv(3, static_cast<int>(dpi), 96);
    const int widestLine = topSize.cx > bottomSize.cx ? topSize.cx : bottomSize.cx;
    return { widestLine + horizontalPadding * 2, textMetrics.tmHeight * 2 + verticalPadding * 2 };
}

bool RateStrip::PlaceBesideNotificationArea(HWND taskbar, HWND notificationArea) noexcept
{
    RECT notificationRect{};
    RECT taskbarClientRect{};
    if (!::GetWindowRect(notificationArea, &notificationRect) ||
        !::GetClientRect(taskbar, &taskbarClientRect))
    {
        return false;
    }

    ::MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&notificationRect), 2);
    const UINT dpi = GetDpiForWindow(taskbar);
    const int margin = MulDiv(4, static_cast<int>(dpi), 96);
    const int availableWidth = notificationRect.left - margin - taskbarClientRect.left;
    const int availableHeight = taskbarClientRect.bottom - taskbarClientRect.top;
    if (availableWidth <= 0 || availableHeight <= 0)
    {
        return false;
    }

    // Keep every supported font applicable even when notification chrome
    // leaves less room than its natural two-line extent.
    size_.cx = std::min(static_cast<int>(naturalSize_.cx), availableWidth);
    size_.cy = std::min(static_cast<int>(naturalSize_.cy), availableHeight);
    const int x = notificationRect.left - size_.cx - margin;
    const int y = taskbarClientRect.top + (availableHeight - size_.cy) / 2;

    textColor_ = theme::ReadMode() == theme::Mode::Light
        ? RGB(24, 24, 24)
        : RGB(255, 255, 255);
    return ::SetWindowPos(
               GetSafeHwnd(),
               HWND_TOP,
               x,
               y,
               size_.cx,
               size_.cy,
               SWP_NOACTIVATE) != FALSE;
}
bool RateStrip::Render() noexcept
{
    if (floating_)
    {
        RateFontSelection selection;
        return GetSafeHwnd() != nullptr && GetRateFont(selection) &&
            floatingRenderer_.Render(uploadText_, downloadText_, selection.logFont,
                GetDpiForWindow(GetSafeHwnd()), pulseTimerActive_ ? pulsePhase_ : 0.5, textOpacity_) && floatingRenderer_.Present(GetSafeHwnd(), floatingShadow_.GetSafeHwnd());
    }
    if (GetSafeHwnd() == nullptr || size_.cx <= 0 || size_.cy <= 0 || font_.GetSafeHandle() == nullptr)
    {
        return false;
    }

    const HDC screenDc = ::GetDC(nullptr);
    if (screenDc == nullptr)
    {
        return false;
    }

    const HDC memoryDc = ::CreateCompatibleDC(screenDc);
    if (memoryDc == nullptr)
    {
        ::ReleaseDC(nullptr, screenDc);
        return false;
    }

    BITMAPINFO bitmapInfo{};
    bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
    bitmapInfo.bmiHeader.biWidth = size_.cx;
    bitmapInfo.bmiHeader.biHeight = -size_.cy;
    bitmapInfo.bmiHeader.biPlanes = 1;
    bitmapInfo.bmiHeader.biBitCount = 32;
    bitmapInfo.bmiHeader.biCompression = BI_RGB;

    void* bitmapBits = nullptr;
    const HBITMAP bitmap = ::CreateDIBSection(memoryDc, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits, nullptr, 0);
    if (bitmap == nullptr || bitmapBits == nullptr)
    {
        ::DeleteDC(memoryDc);
        ::ReleaseDC(nullptr, screenDc);
        return false;
    }

    const HGDIOBJ previousBitmap = ::SelectObject(memoryDc, bitmap);
    const HGDIOBJ previousFont = ::SelectObject(memoryDc, font_.GetSafeHandle());
    const RECT clientRect{ 0, 0, size_.cx, size_.cy };
    auto* const pixels = static_cast<DWORD*>(bitmapBits);
    const size_t pixelCount = static_cast<size_t>(size_.cx) * static_cast<size_t>(size_.cy);
    std::fill_n(pixels, pixelCount, 0u);
    ::PatBlt(memoryDc, 0, 0, size_.cx, size_.cy, BLACKNESS);
    if (!floating_)
    {
        ::SetBkMode(memoryDc, TRANSPARENT);
        ::SetTextColor(memoryDc, RGB(255, 255, 255));
        const int midpoint = size_.cy / 2;
        RECT topLineRect = clientRect;
        topLineRect.bottom = midpoint;
        RECT bottomLineRect = clientRect;
        bottomLineRect.top = midpoint;
        const std::wstring topLine = uploadText_ + L" \u2191";
        const std::wstring bottomLine = downloadText_ + L" \u2193";
        ::DrawTextW(memoryDc, topLine.c_str(), -1, &topLineRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
        ::DrawTextW(memoryDc, bottomLine.c_str(), -1, &bottomLineRect, DT_RIGHT | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    }

    const DWORD red = GetRValue(textColor_);
    const DWORD green = GetGValue(textColor_);
    const DWORD blue = GetBValue(textColor_);
    {
        // A layered window passes mouse input through fully transparent pixels, so
        // an interactive strip needs a floor that is hit-testable yet unnoticeable.
        const DWORD minimumAlpha = contextMenuEnabled_ ? 1u : 0u;
        for (size_t index = 0; index < pixelCount; ++index)
        {
            const DWORD mask = pixels[index];
            const DWORD alpha = std::max<DWORD>(
                minimumAlpha, std::max(GetRValue(mask), std::max(GetGValue(mask), GetBValue(mask))));
            pixels[index] =
                (alpha << 24) |
                ((red * alpha / 255) << 16) |
                ((green * alpha / 255) << 8) |
                (blue * alpha / 255);
        }
    }

    POINT source{};
    SIZE windowSize{ size_.cx, size_.cy };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    const bool rendered = ::UpdateLayeredWindow(
                              GetSafeHwnd(),
                              screenDc,
                              nullptr,
                              &windowSize,
                              memoryDc,
                              &source,
                              0,
                              &blend,
                              ULW_ALPHA) != FALSE;

    ::SelectObject(memoryDc, previousFont);
    ::SelectObject(memoryDc, previousBitmap);
    ::DeleteObject(bitmap);
    ::DeleteDC(memoryDc);
    ::ReleaseDC(nullptr, screenDc);
    return rendered;
}

void RateStrip::OnPaint()
{
    CPaintDC deviceContext(this);
    static_cast<void>(Render());
}

BOOL RateStrip::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

int RateStrip::OnMouseActivate(CWnd*, UINT, UINT)
{
    return MA_NOACTIVATE;
}

LRESULT RateStrip::OnNcHitTest(CPoint point)
{
    if (floating_)
    {
        ScreenToClient(&point);
        return FloatingRateRenderer::HitTest(point, GetDpiForWindow(GetSafeHwnd())) ? HTCLIENT : HTTRANSPARENT;
    }
    return floating_ || contextMenuEnabled_ ? HTCLIENT : HTTRANSPARENT;
}

void RateStrip::OnRButtonUp(UINT flags, CPoint point)
{
    if (!contextMenuEnabled_ || contextMenuOwner_ == nullptr || contextMenuMessage_ == 0)
    {
        CWnd::OnRButtonUp(flags, point);
        return;
    }

    // Post so the owner opens its menu outside this mouse message.
    ::PostMessageW(contextMenuOwner_, contextMenuMessage_, 0, 0);
}

void RateStrip::OnLButtonDown(UINT flags, CPoint point)
{
    if (!floating_)
    {
        CWnd::OnLButtonDown(flags, point);
        return;
    }
    ClientToScreen(&point);
    ReleaseCapture();
    SendMessageW(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
}

void RateStrip::OnExitSizeMove()
{
    if (floating_ && positionChangedOwner_ != nullptr && positionChangedMessage_ != 0)
    {
        ::PostMessageW(positionChangedOwner_, positionChangedMessage_, 0, 0);
    }
    CWnd::OnExitSizeMove();
}

POINT RateStrip::GetFloatingPosition() const noexcept
{
    RECT windowRect{};
    return GetSafeHwnd() != nullptr && ::GetWindowRect(GetSafeHwnd(), &windowRect)
        ? POINT{windowRect.left, windowRect.top}
        : POINT{};
}

LRESULT RateStrip::OnDpiChanged(WPARAM, LPARAM parameter)
{
    if (floating_)
    {
        const auto* rect = reinterpret_cast<const RECT*>(parameter);
        ::SetWindowPos(GetSafeHwnd(), HWND_TOPMOST, rect->left, rect->top, 0, 0,
            SWP_NOSIZE | SWP_NOACTIVATE);
        size_ = FloatingRateRenderer::SizeForDpi(GetDpiForWindow(GetSafeHwnd()));
        static_cast<void>(Render());
        if (positionChangedOwner_ && positionChangedMessage_)
            ::PostMessageW(positionChangedOwner_, positionChangedMessage_, 0, 0);
    }
    return 0;
}

void RateStrip::OnDisplayChange(UINT, int, int)
{
    if (floating_ && RelayoutFloating())
    {
        static_cast<void>(Render());
        if (positionChangedOwner_ && positionChangedMessage_)
            ::PostMessageW(positionChangedOwner_, positionChangedMessage_, 0, 0);
    }
}

void RateStrip::OnWindowPosChanged(WINDOWPOS* position)
{
    CWnd::OnWindowPosChanged(position);
    if (floating_ && floatingShadow_.GetSafeHwnd())
    {
        RECT rect{};
        if (::GetWindowRect(GetSafeHwnd(), &rect))
            floatingShadow_.SetWindowPos(&wndTopMost, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void RateStrip::UpdatePulseTimer() noexcept
{
    if (!floating_ || !GetSafeHwnd()) return;
    BOOL animations = FALSE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    const float targetOpacity = hovering_ ? 1.0f : 0.0f;
    if (!animations) textOpacity_ = targetOpacity;
    const bool animate = IsWindowVisible() && animations &&
        (floatingRenderer_.Activity() > 0 || textOpacity_ != targetOpacity);
    if (animate && !pulseTimerActive_)
    {
        pulseTick_ = GetTickCount64();
        pulseTimerActive_ = SetTimer(1, 33, nullptr) != 0;
    }
    else if (!animate && pulseTimerActive_)
    {
        KillTimer(1);
        pulseTimerActive_ = false;
        pulsePhase_ = 0;
    }
}

void RateStrip::OnTimer(UINT_PTR timer)
{
    if (timer == 1 && floating_ && pulseTimerActive_)
    {
        if (!IsWindowVisible())
        {
            UpdatePulseTimer();
            return;
        }
        const ULONGLONG now = GetTickCount64();
        const float step = static_cast<float>(now - pulseTick_) / (hovering_ ? 180.0f : 220.0f);
        textOpacity_ = hovering_ ? std::min(1.0f, textOpacity_ + step) : std::max(0.0f, textOpacity_ - step);
        const double period = 2200 - 1200 * floatingRenderer_.Activity();
        pulsePhase_ = std::fmod(pulsePhase_ + (now - pulseTick_) / period, 1.0);
        pulseTick_ = now;
        UpdatePulseTimer();
        static_cast<void>(Render());
        return;
    }
    CWnd::OnTimer(timer);
}

void RateStrip::OnSettingChange(UINT flags, LPCTSTR section)
{
    UpdatePulseTimer();
    if (floating_ && IsWindowVisible()) static_cast<void>(Render());
    CWnd::OnSettingChange(flags, section);
}

void RateStrip::OnMouseMove(UINT flags, CPoint point)
{
    if (floating_ && !hovering_)
    {
        TRACKMOUSEEVENT tracking{sizeof(tracking), TME_LEAVE, GetSafeHwnd(), 0};
        if (::TrackMouseEvent(&tracking))
        {
            hovering_ = true;
            UpdatePulseTimer();
            static_cast<void>(Render());
        }
    }
    CWnd::OnMouseMove(flags, point);
}

void RateStrip::OnMouseLeave()
{
    if (floating_)
    {
        hovering_ = false;
        UpdatePulseTimer();
        static_cast<void>(Render());
    }
    CWnd::OnMouseLeave();
}
