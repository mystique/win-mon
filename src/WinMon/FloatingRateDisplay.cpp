#include "FloatingRateDisplay.h"
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

void FloatingRateDisplay::SetRates(
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
    ON_WM_MOUSEMOVE()
    ON_WM_MOUSELEAVE()
    ON_WM_TIMER()
    ON_WM_SETTINGCHANGE()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEACTIVATE()
    ON_WM_NCHITTEST()
    ON_WM_RBUTTONUP()
    ON_WM_LBUTTONDOWN()
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
        return true;
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

bool FloatingRateDisplay::RelayoutFloating() noexcept
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

void FloatingRateDisplay::Shutdown() noexcept
{
    if (GetSafeHwnd() && pulseTimerActive_) KillTimer(1);
    pulseTimerActive_ = false;
    pulsePhase_ = 0;
    hovering_ = false;
    hoverProgress_ = 0;
    if (GetSafeHwnd()) DestroyWindow();
    if (floatingShadow_.GetSafeHwnd()) floatingShadow_.DestroyWindow();
    floatingRenderer_.ReleaseTarget();
    size_ = {};
}

bool FloatingRateDisplay::UpdateRateFont(std::optional<RateFontSelection> selection) noexcept
{
    return rateFont_.Apply(selection, [this] {
        return GetSafeHwnd() == nullptr || (RelayoutFloating() && Render());
    });
}

bool FloatingRateDisplay::Render() noexcept
{
    RateFontSelection selection;
    return GetSafeHwnd() != nullptr && GetRateFont(selection) &&
        floatingRenderer_.Render(uploadText_, downloadText_, selection.logFont,
            GetDpiForWindow(GetSafeHwnd()), pulseTimerActive_ ? pulsePhase_ : 0.5, hoverProgress_) &&
        floatingRenderer_.Present(GetSafeHwnd(), floatingShadow_.GetSafeHwnd());
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
    return FloatingRateRenderer::HitTest(point, GetDpiForWindow(GetSafeHwnd())) ? HTCLIENT : HTTRANSPARENT;
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
    size_ = FloatingRateRenderer::SizeForDpi(GetDpiForWindow(GetSafeHwnd()));
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
    const float targetOpacity = hovering_ ? 1.0f : 0.0f;
    if (!animations) hoverProgress_ = targetOpacity;
    const bool animate = IsWindowVisible() && animations &&
        (floatingRenderer_.Activity() > 0 || hoverProgress_ != targetOpacity);
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

void FloatingRateDisplay::OnTimer(UINT_PTR timer)
{
    if (timer == 1 && pulseTimerActive_)
    {
        if (!IsWindowVisible())
        {
            UpdatePulseTimer();
            return;
        }
        const ULONGLONG now = GetTickCount64();
        const float step = static_cast<float>(now - pulseTick_) / (hovering_ ? 180.0f : 220.0f);
        hoverProgress_ = hovering_ ? std::min(1.0f, hoverProgress_ + step) : std::max(0.0f, hoverProgress_ - step);
        const double period = 2200 - 1200 * floatingRenderer_.Activity();
        pulsePhase_ = std::fmod(pulsePhase_ + (now - pulseTick_) / period, 1.0);
        pulseTick_ = now;
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

void FloatingRateDisplay::OnMouseMove(UINT flags, CPoint point)
{
    if (!hovering_)
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

void FloatingRateDisplay::OnMouseLeave()
{
    hovering_ = false;
    UpdatePulseTimer();
    static_cast<void>(Render());
    CWnd::OnMouseLeave();
}
