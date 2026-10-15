// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#include "taskbar/RateStrip.h"
#include "platform/Theme.h"
#include "taskbar/LedRateStrip.h"

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

bool RateStrip::SetTaskbarDisplayOnLeft(bool left) noexcept
{
    if (taskbarDisplayOnLeft_ == left) return true;
    taskbarDisplayOnLeft_ = left;
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
    if (notificationArea == nullptr && !taskbarDisplayOnLeft_)
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


    naturalSize_ = MeasureSize(taskbar);
    size_ = naturalSize_;
    if (size_.cx <= 0 || size_.cy <= 0)
    {
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
        return false;
    }

    // SetParent leaves popup styles intact; embedding needs child coordinates and hit testing.
    if (!ModifyStyle(WS_POPUP | WS_SYSMENU, WS_CHILD) ||
        ::SetParent(GetSafeHwnd(), taskbar) == nullptr)
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
    if (notificationArea == nullptr && !taskbarDisplayOnLeft_)
    {
        // Windows 11 may temporarily make TrayNotifyWnd unavailable in the
        // taskbar child tree while Start is open. Keep the existing strip at
        // its last valid position; SetRates renders and the next sample retries.
        return true;
    }

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

CSize RateStrip::MeasureSize(HWND taskbar)
{
    const UINT dpi = GetDpiForWindow(taskbar);
    const SIZE size = ledStrip::Size(dpi);
    return {size.cx, size.cy};
}

bool RateStrip::PlaceOnTaskbar(HWND taskbar, HWND notificationArea) noexcept
{
    RECT notificationRect{};
    RECT taskbarClientRect{};
    if (!::GetClientRect(taskbar, &taskbarClientRect) ||
        (!taskbarDisplayOnLeft_ && (notificationArea == nullptr || !::GetWindowRect(notificationArea, &notificationRect))))
    {
        visible_ = false;
        return false;
    }

    ::MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&notificationRect), 2);
    const UINT dpi = GetDpiForWindow(taskbar);
    const int margin = MulDiv(4, static_cast<int>(dpi), 96);
    const int availableWidth = taskbarDisplayOnLeft_
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
    const int x = taskbarDisplayOnLeft_ ? taskbarClientRect.left + margin : notificationRect.left - size_.cx - margin;
    const int y = taskbarClientRect.top + (availableHeight - size_.cy) / 2;
    const RECT candidate{x, y, x + size_.cx, y + size_.cy};
    if (taskbarDisplayOnLeft_ && !taskbarSpace_.IsFree(taskbar, candidate, GetSafeHwnd()))
    {
        ShowWindow(SW_HIDE);
        return true;
    }

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
    if (GetSafeHwnd() == nullptr || size_.cx <= 0 || size_.cy <= 0)
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
    auto* const pixels = static_cast<DWORD*>(bitmapBits);
    const size_t pixelCount = static_cast<size_t>(size_.cx) * static_cast<size_t>(size_.cy);
    std::fill_n(pixels, pixelCount, 0u);
    bool rendered = ledStrip::Render(memoryDc, {size_.cx, size_.cy}, uploadText_, downloadText_,
        taskbarDisplayOnLeft_, theme::ReadMode() == theme::Mode::Light);
    if (rendered && contextMenuEnabled_)
        for (size_t index = 0; index < pixelCount; ++index)
            if (!(pixels[index] >> 24)) pixels[index] = 0x01000000;

    POINT source{};
    SIZE windowSize{ size_.cx, size_.cy };
    BLENDFUNCTION blend{ AC_SRC_OVER, 0, 255, AC_SRC_ALPHA };
    rendered = rendered && ::UpdateLayeredWindow(
                              GetSafeHwnd(),
                              screenDc,
                              nullptr,
                              &windowSize,
                              memoryDc,
                              &source,
                              0,
                              &blend,
                              ULW_ALPHA) != FALSE;

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
