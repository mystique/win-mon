// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#include "taskbar/RateStrip.h"
#include "platform/Theme.h"

#include <algorithm>
#include <array>

#include <cwchar>
#include <iterator>

namespace
{
constexpr wchar_t kTaskbarClassName[] = L"Shell_TrayWnd";
constexpr wchar_t kNotificationAreaClassName[] = L"TrayNotifyWnd";
struct NotificationAreaSearch
{
    HWND notificationArea = nullptr;
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

}

void RateStrip::SetRates(
    const std::wstring& uploadText,
    const std::wstring& downloadText) noexcept
{
    uploadText_ = uploadText;
    downloadText_ = downloadText;
    if (GetSafeHwnd() != nullptr)
    {
        static_cast<void>(Render());
    }
}

bool RateStrip::SetSpeedTextOnLeft(bool left) noexcept
{
    if (speedTextOnLeft_ == left) return true;
    speedTextOnLeft_ = left;
    if (GetSafeHwnd() == nullptr) return true;
    const HWND taskbar = FindPrimaryBottomTaskbar();
    if (taskbar == nullptr || taskbar_ != taskbar)
    {
        Shutdown();
        return true;
    }
    ShowWindow(SW_HIDE);
    return Relayout(taskbar, FindNotificationArea(taskbar)) && Render();
}

std::wstring RateStrip::GetRateFontName() const
{
    RateFontSelection selection;
    return GetRateFont(selection) ? selection.logFont.lfFaceName : L"";
}

bool RateStrip::GetRateFont(RateFontSelection& selection) const noexcept
{
    return rateFont_.Get(taskbar_ != nullptr && IsWindow(taskbar_) ? taskbar_ : nullptr, selection);
}

bool RateStrip::UpdateRateFont(std::optional<RateFontSelection> selection) noexcept
{
    return rateFont_.Apply(selection, [this] {
        if (GetSafeHwnd() == nullptr) return true;
        const HWND taskbar = FindPrimaryBottomTaskbar();
        const HWND area = taskbar == nullptr ? nullptr : FindNotificationArea(taskbar);
        return taskbar != nullptr && taskbar_ == taskbar && Relayout(taskbar, area) && Render();
    });
}

void RateStrip::SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept
{
    contextMenuOwner_ = owner;
    contextMenuMessage_ = notificationMessage;
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
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEACTIVATE()
    ON_WM_NCHITTEST()
    ON_WM_RBUTTONUP()
END_MESSAGE_MAP()
bool RateStrip::Embed(bool shouldShow)
{
    if (!shouldShow)
    {
        Shutdown();
        return false;
    }
    const HWND taskbar = FindPrimaryBottomTaskbar();
    if (taskbar == nullptr)
    {
        Shutdown();
        return false;
    }

    const HWND notificationArea = FindNotificationArea(taskbar);
    if (notificationArea == nullptr && !speedTextOnLeft_)
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
            ShowWindow(visible_ ? SW_SHOWNOACTIVATE : SW_HIDE);
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

    ShowWindow(visible_ ? SW_SHOWNOACTIVATE : SW_HIDE);
    return Render();
}

bool RateStrip::Refresh()
{
    if (GetSafeHwnd() == nullptr)
    {
        return false;
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
    if (notificationArea == nullptr && !speedTextOnLeft_)
    {
        // Windows 11 may temporarily make TrayNotifyWnd unavailable in the
        // taskbar child tree while Start is open. Keep the existing strip at
        // its last valid position; SetRates renders and the next sample retries.
        return true;
    }

    ShowWindow(SW_HIDE);
    if (!PlaceOnTaskbar(taskbar, notificationArea))
    {
        Shutdown();
        return false;
    }

    if (!Render())
    {
        Shutdown();
        return false;
    }

    ShowWindow(visible_ ? SW_SHOWNOACTIVATE : SW_HIDE);
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
    return PlaceOnTaskbar(taskbar, notificationArea);
}

void RateStrip::Shutdown() noexcept
{
    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }

    if (font_.GetSafeHandle() != nullptr)
    {
        font_.DeleteObject();
    }

    taskbar_ = nullptr;
    size_ = {};
    visible_ = false;
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

bool RateStrip::CreateRateFont(HWND taskbar) noexcept
{
    RateFontSelection selection;
    if (!rateFont_.Get(taskbar, selection))
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

CSize RateStrip::MeasureSize(HWND taskbar)
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

    const bool measured = layout_.Measure(deviceContext, GetDpiForWindow(taskbar));

    SelectObject(deviceContext, previousFont);
    ::ReleaseDC(taskbar, deviceContext);

    if (!measured)
    {
        return {};
    }

    return {layout_.width, layout_.height};
}

bool RateStrip::PlaceOnTaskbar(HWND taskbar, HWND notificationArea) noexcept
{
    RECT notificationRect{};
    RECT taskbarClientRect{};
    if (!::GetClientRect(taskbar, &taskbarClientRect) ||
        (!speedTextOnLeft_ && (notificationArea == nullptr || !::GetWindowRect(notificationArea, &notificationRect))))
    {
        visible_ = false;
        return false;
    }

    ::MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&notificationRect), 2);
    const UINT dpi = GetDpiForWindow(taskbar);
    const int margin = MulDiv(4, static_cast<int>(dpi), 96);
    const int availableWidth = speedTextOnLeft_
        ? taskbarClientRect.right - taskbarClientRect.left - margin * 2
        : notificationRect.left - margin - taskbarClientRect.left;
    const int availableHeight = taskbarClientRect.bottom - taskbarClientRect.top;
    size_ = naturalSize_;
    visible_ = false;
    if (naturalSize_.cx > availableWidth || naturalSize_.cy > availableHeight || availableWidth <= 0 || availableHeight <= 0)
    {
        ShowWindow(SW_HIDE);
        return true;
    }
    const int x = speedTextOnLeft_ ? taskbarClientRect.left + margin : notificationRect.left - size_.cx - margin;
    const int y = taskbarClientRect.top + (availableHeight - size_.cy) / 2;
    const RECT candidate{x, y, x + size_.cx, y + size_.cy};
    if (speedTextOnLeft_ && !taskbarSpace_.IsFree(taskbar, candidate, GetSafeHwnd()))
    {
        ShowWindow(SW_HIDE);
        return true;
    }

    textColor_ = theme::ReadMode() == theme::Mode::Light
        ? RGB(24, 24, 24)
        : RGB(255, 255, 255);
    visible_ = ::SetWindowPos(
               GetSafeHwnd(),
               HWND_TOP,
               x,
               y,
               size_.cx,
               size_.cy,
               SWP_NOACTIVATE) != FALSE;
    if (visible_) ShowWindow(SW_SHOWNOACTIVATE);
    return visible_;
}

bool RateStrip::Render() noexcept
{
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
    auto* const pixels = static_cast<DWORD*>(bitmapBits);
    const size_t pixelCount = static_cast<size_t>(size_.cx) * static_cast<size_t>(size_.cy);
    std::fill_n(pixels, pixelCount, 0u);
    ::PatBlt(memoryDc, 0, 0, size_.cx, size_.cy, BLACKNESS);
    {
        ::SetBkMode(memoryDc, TRANSPARENT);
        ::SetTextColor(memoryDc, RGB(255, 255, 255));
        const int midpoint = size_.cy / 2;
        const bool drawn = layout_.Draw(memoryDc, uploadText_, L"\u2191", layout_.Columns(speedTextOnLeft_, 0, midpoint)) &&
            layout_.Draw(memoryDc, downloadText_, L"\u2193", layout_.Columns(speedTextOnLeft_, midpoint, size_.cy));
        if (!drawn)
        {
            ::SelectObject(memoryDc, previousFont);
            ::SelectObject(memoryDc, previousBitmap);
            ::DeleteObject(bitmap);
            ::DeleteDC(memoryDc);
            ::ReleaseDC(nullptr, screenDc);
            return false;
        }
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

LRESULT RateStrip::OnNcHitTest(CPoint)
{
    return contextMenuEnabled_ ? HTCLIENT : HTTRANSPARENT;
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
