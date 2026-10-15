// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#include "floating/FloatingRateDisplay.h"
#include <algorithm>
#include <cmath>

namespace
{
struct FloatingVisibilitySearch
{
    RECT target{};
    bool visible = false;
};

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

bool FloatingRateDisplay::SetTextVisible(bool visible) noexcept
{
    return SetVisible(true, visible);
}

bool FloatingRateDisplay::SetChartVisible(bool visible) noexcept
{
    return SetVisible(false, visible);
}

bool FloatingRateDisplay::SetVisible(bool text, bool visible) noexcept
{
    const bool current = text ? floatingRenderer_.IsTextVisible() : floatingRenderer_.IsChartVisible();
    const auto snapshot = floatingRenderer_.CaptureVisibilityState();
    const bool windowVisible = GetSafeHwnd() != nullptr && IsWindowVisible();
    BOOL animations = FALSE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    floatingRenderer_.BeginVisibility(text, visible, GetTickCount64(), windowVisible, animations != FALSE);
    if (visible == current)
    {
        if (GetSafeHwnd() == nullptr || Render()) return true;
        floatingRenderer_.RestoreVisibilityState(snapshot);
        return false;
    }
    if (!windowVisible) return true;
    const POINT previousPosition = GetFloatingPosition();
    UpdatePulseTimer();
    if (Render()) return true;
    floatingRenderer_.RestoreVisibilityState(snapshot);
    ::SetWindowPos(GetSafeHwnd(), nullptr, previousPosition.x, previousPosition.y, 0, 0,
        SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    UpdatePulseTimer();
    static_cast<void>(Render());
    return false;
}

bool FloatingRateDisplay::SetOpacity(int percent) noexcept
{
    if (percent < 10 || percent > 100) return false;
    const int previous = opacity_;
    opacity_ = percent;
    if (!GetSafeHwnd() || Render()) return true;
    opacity_ = previous;
    static_cast<void>(Render());
    return false;
}

void FloatingRateDisplay::SetRates(
    const std::wstring& uploadText,
    const std::wstring& downloadText,
    double uploadBytesPerSecond,
    double downloadBytesPerSecond) noexcept
{
    floatingRenderer_.SetRates(uploadText, downloadText, uploadBytesPerSecond, downloadBytesPerSecond, GetTickCount64());
    UpdatePulseTimer();
    if (GetSafeHwnd() != nullptr)
    {
        static_cast<void>(Render());
    }
}

void FloatingRateDisplay::SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept
{
    contextMenuOwner_ = owner;
    contextMenuMessage_ = notificationMessage;
}

void FloatingRateDisplay::SetPositionChangedOwner(HWND owner, UINT notificationMessage) noexcept
{
    positionChangedOwner_ = owner;
    positionChangedMessage_ = notificationMessage;
}

BEGIN_MESSAGE_MAP(FloatingRateDisplay, CWnd)
    ON_WM_PAINT()
    ON_WM_TIMER()
    ON_WM_SETTINGCHANGE()
    ON_WM_SHOWWINDOW()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEACTIVATE()
    ON_WM_NCHITTEST()
    ON_WM_RBUTTONUP()
    ON_WM_LBUTTONDOWN()
    ON_WM_LBUTTONDBLCLK()
    ON_WM_MOVING()
    ON_WM_EXITSIZEMOVE()
    ON_WM_WINDOWPOSCHANGED()
    ON_MESSAGE(WM_DPICHANGED, &FloatingRateDisplay::OnDpiChanged)
    ON_WM_DISPLAYCHANGE()
END_MESSAGE_MAP()

bool FloatingRateDisplay::ShowFloating(POINT position, bool restorePosition)
{
    if (GetSafeHwnd() != nullptr)
    {
        ShowWindow(SW_SHOWNOACTIVATE);
        floatingShadow_.ShowWindow(SW_SHOWNOACTIVATE);
        UpdatePulseTimer();
        return true;
    }

    size_ = floatingRenderer_.CurrentSizeForDpi(GetDpiForSystem());
    if (!restorePosition) position = DefaultFloatingPosition(size_);

    const CString windowClass = AfxRegisterWndClass(CS_DBLCLKS, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr);
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
    UpdatePulseTimer();
    return true;
}

bool FloatingRateDisplay::RelayoutFloating() noexcept
{
    RECT windowRect{};
    if (GetSafeHwnd() == nullptr || !::GetWindowRect(GetSafeHwnd(), &windowRect))
    {
        return false;
    }
    UINT dpi = GetDpiForWindow(GetSafeHwnd());
    size_ = floatingRenderer_.CurrentSizeForDpi(dpi);
    POINT position{windowRect.left, windowRect.top};
    if (!IsFloatingPositionVisible(position, size_, dpi))
        position = DefaultFloatingPosition(size_);
    if (!::SetWindowPos(GetSafeHwnd(), HWND_TOPMOST, position.x, position.y, 0, 0,
            SWP_NOACTIVATE | SWP_NOSIZE)) return false;
    size_ = floatingRenderer_.CurrentSizeForDpi(GetDpiForWindow(GetSafeHwnd()));
    return true;
}

void FloatingRateDisplay::Shutdown() noexcept
{
    if (GetSafeHwnd() && pulseTimerActive_) KillTimer(1);
    pulseTimerActive_ = false;
    floatingRenderer_.FinishTransitions(true);
    if (GetSafeHwnd()) DestroyWindow();
    if (floatingShadow_.GetSafeHwnd()) floatingShadow_.DestroyWindow();
    floatingRenderer_.ReleaseTarget();
    size_ = {};
}

bool FloatingRateDisplay::Render() noexcept
{
    if (!GetSafeHwnd()) return false;
    size_ = floatingRenderer_.CurrentSizeForDpi(GetDpiForWindow(GetSafeHwnd()));
    RECT rect{};
    MONITORINFO monitor{sizeof(monitor)};
    if (!::GetWindowRect(GetSafeHwnd(), &rect)) return false;
    if (GetMonitorInfoW(MonitorFromWindow(GetSafeHwnd(), MONITOR_DEFAULTTONEAREST), &monitor))
    {
        const LONG x = std::clamp(rect.left, monitor.rcWork.left, std::max(monitor.rcWork.left, monitor.rcWork.right - size_.cx));
        const LONG y = std::clamp(rect.top, monitor.rcWork.top, std::max(monitor.rcWork.top, monitor.rcWork.bottom - size_.cy));
        positionDirty_ |= x != rect.left || y != rect.top;
        if (!::SetWindowPos(GetSafeHwnd(), nullptr, x, y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE)) return false;
    }
    const bool rendered = floatingRenderer_.RenderCurrent(GetDpiForWindow(GetSafeHwnd()), pulseTimerActive_) &&
        floatingRenderer_.Present(GetSafeHwnd(), floatingShadow_.GetSafeHwnd(), static_cast<BYTE>(MulDiv(opacity_, 255, 100)));
    if (rendered && positionDirty_ && !floatingRenderer_.HasActiveVisibilityTransition())
    {
        if (positionChangedOwner_ && positionChangedMessage_)
            ::PostMessageW(positionChangedOwner_, positionChangedMessage_, 0, 0);
        positionDirty_ = false;
    }
    return rendered;
}

void FloatingRateDisplay::OnPaint()
{
    CPaintDC deviceContext(this);
    static_cast<void>(Render());
}

BOOL FloatingRateDisplay::OnEraseBkgnd(CDC*)
{
    return TRUE;
}

int FloatingRateDisplay::OnMouseActivate(CWnd*, UINT, UINT)
{
    return MA_NOACTIVATE;
}

LRESULT FloatingRateDisplay::OnNcHitTest(CPoint point)
{
    ScreenToClient(&point);
    return floatingRenderer_.HitTestCurrent(point, GetDpiForWindow(GetSafeHwnd())) ? HTCLIENT : HTTRANSPARENT;
}

void FloatingRateDisplay::OnRButtonUp(UINT flags, CPoint point)
{
    if (contextMenuOwner_ == nullptr || contextMenuMessage_ == 0)
    {
        CWnd::OnRButtonUp(flags, point);
        return;
    }

    // Post so the owner opens its menu outside this mouse message.
    ::PostMessageW(contextMenuOwner_, contextMenuMessage_, 0, 0);
}

void FloatingRateDisplay::OnLButtonDown(UINT, CPoint point)
{
    ClientToScreen(&point);
    ReleaseCapture();
    SendMessageW(WM_NCLBUTTONDOWN, HTCAPTION, MAKELPARAM(point.x, point.y));
}

void FloatingRateDisplay::OnLButtonDblClk(UINT, CPoint point)
{
    if (!FloatingRateRenderer::HitTest(point, GetDpiForWindow(GetSafeHwnd()), 0, 0)) return;
    const auto step = floatingRenderer_.NextVisibilityCycleStep();
    const bool applied = visibilityChangeHandler_
        ? visibilityChangeHandler_(step.changeText, step.visible)
        : (step.changeText ? SetTextVisible(step.visible) : SetChartVisible(step.visible));
    floatingRenderer_.CommitVisibilityCycleStep(applied, step.visible, step.previousDirection);
}

void FloatingRateDisplay::OnMoving(UINT side, LPRECT rect)
{
    CWnd::OnMoving(side, rect);

    MONITORINFO monitorInfo{sizeof(monitorInfo)};
    if (!GetMonitorInfoW(MonitorFromRect(rect, MONITOR_DEFAULTTONEAREST), &monitorInfo)) return;
    const RECT work = monitorInfo.rcWork;
    const LONG x = std::clamp(rect->left, work.left,
        std::max(work.left, work.right - (rect->right - rect->left)));
    const LONG y = std::clamp(rect->top, work.top,
        std::max(work.top, work.bottom - (rect->bottom - rect->top)));
    OffsetRect(rect, x - rect->left, y - rect->top);
}

void FloatingRateDisplay::OnExitSizeMove()
{
    if (positionChangedOwner_ != nullptr && positionChangedMessage_ != 0)
    {
        ::PostMessageW(positionChangedOwner_, positionChangedMessage_, 0, 0);
    }
    CWnd::OnExitSizeMove();
}

POINT FloatingRateDisplay::GetFloatingPosition() const noexcept
{
    RECT windowRect{};
    return GetSafeHwnd() != nullptr && ::GetWindowRect(GetSafeHwnd(), &windowRect)
        ? POINT{windowRect.left, windowRect.top}
        : POINT{};
}

LRESULT FloatingRateDisplay::OnDpiChanged(WPARAM, LPARAM parameter)
{
    const auto* rect = reinterpret_cast<const RECT*>(parameter);
    ::SetWindowPos(GetSafeHwnd(), HWND_TOPMOST, rect->left, rect->top, 0, 0,
        SWP_NOSIZE | SWP_NOACTIVATE);
    size_ = floatingRenderer_.CurrentSizeForDpi(GetDpiForWindow(GetSafeHwnd()));
    static_cast<void>(Render());
    if (positionChangedOwner_ && positionChangedMessage_)
        ::PostMessageW(positionChangedOwner_, positionChangedMessage_, 0, 0);
    return 0;
}

void FloatingRateDisplay::OnDisplayChange(UINT, int, int)
{
    if (RelayoutFloating())
    {
        static_cast<void>(Render());
        if (positionChangedOwner_ && positionChangedMessage_)
            ::PostMessageW(positionChangedOwner_, positionChangedMessage_, 0, 0);
    }
}

void FloatingRateDisplay::OnWindowPosChanged(WINDOWPOS* position)
{
    CWnd::OnWindowPosChanged(position);
    if (floatingShadow_.GetSafeHwnd())
    {
        RECT rect{};
        if (::GetWindowRect(GetSafeHwnd(), &rect))
            floatingShadow_.SetWindowPos(&wndTopMost, rect.left, rect.top, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
    }
}

void FloatingRateDisplay::UpdatePulseTimer() noexcept
{
    if (!GetSafeHwnd()) return;
    BOOL animations = FALSE;
    SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
    const bool animate = floatingRenderer_.SyncAnimationState(IsWindowVisible() != FALSE, animations != FALSE);
    if (animate && !pulseTimerActive_)
    {
        floatingRenderer_.StartAnimationClock(GetTickCount64());
        pulseTimerActive_ = SetTimer(1, 33, nullptr) != 0;
        if (!pulseTimerActive_)
            static_cast<void>(floatingRenderer_.SyncAnimationState(IsWindowVisible() != FALSE, false));
    }
    else if (!animate && pulseTimerActive_)
    {
        KillTimer(1);
        pulseTimerActive_ = false;
        floatingRenderer_.StopAnimationClock();
    }
}

void FloatingRateDisplay::OnTimer(UINT_PTR timer)
{
    if (timer == 1 && pulseTimerActive_)
    {
        if (!IsWindowVisible())
        {
            UpdatePulseTimer();
            return;
        }
        BOOL animations = FALSE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
        static_cast<void>(floatingRenderer_.StepAnimation(GetTickCount64(), IsWindowVisible() != FALSE, animations != FALSE));
        UpdatePulseTimer();
        static_cast<void>(Render());
        return;
    }
    CWnd::OnTimer(timer);
}

void FloatingRateDisplay::OnSettingChange(UINT flags, LPCTSTR section)
{
    UpdatePulseTimer();
    if (IsWindowVisible()) static_cast<void>(Render());
    CWnd::OnSettingChange(flags, section);
}

void FloatingRateDisplay::OnShowWindow(BOOL show, UINT status)
{
    CWnd::OnShowWindow(show, status);
    if (!show)
    {
        if (pulseTimerActive_) KillTimer(1);
        pulseTimerActive_ = false;
        floatingRenderer_.FinishTransitions(false);
        if (floatingShadow_.GetSafeHwnd()) floatingShadow_.ShowWindow(SW_HIDE);
    }
    else
    {
        static_cast<void>(Render());
        if (floatingShadow_.GetSafeHwnd()) floatingShadow_.ShowWindow(SW_SHOWNOACTIVATE);
        UpdatePulseTimer();
    }
}
