#include "RateStrip.h"

#include <iterator>
#include <cwchar>

namespace
{
constexpr wchar_t kTaskbarClassName[] = L"Shell_TrayWnd";
constexpr wchar_t kNotificationAreaClassName[] = L"TrayNotifyWnd";
constexpr wchar_t kTopLine[] = L"0.0K/s ↑";
constexpr wchar_t kBottomLine[] = L"0.0K/s ↓";
constexpr wchar_t kMaximumTopLine[] = L"999.9G/s ↑";
constexpr wchar_t kMaximumBottomLine[] = L"999.9G/s ↓";

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

BEGIN_MESSAGE_MAP(RateStrip, CWnd)
    ON_WM_PAINT()
    ON_WM_ERASEBKGND()
    ON_WM_MOUSEACTIVATE()
    ON_WM_NCHITTEST()
END_MESSAGE_MAP()

bool RateStrip::Embed()
{
    if (GetSafeHwnd() != nullptr)
    {
        return true;
    }

    const HWND taskbar = FindPrimaryBottomTaskbar();
    if (taskbar == nullptr)
    {
        return false;
    }

    const HWND notificationArea = FindNotificationArea(taskbar);
    if (notificationArea == nullptr)
    {
        return false;
    }

    if (!CreateSystemUiFont(taskbar))
    {
        return false;
    }

    size_ = MeasureSize(taskbar);
    if (size_.cx <= 0 || size_.cy <= 0)
    {
        font_.DeleteObject();
        return false;
    }

    const CString windowClass = AfxRegisterWndClass(0, LoadCursorW(nullptr, IDC_ARROW), nullptr, nullptr);
    const DWORD style = WS_CHILD | WS_DISABLED | WS_CLIPSIBLINGS;
    if (!CreateEx(
            WS_EX_NOACTIVATE,
            windowClass,
            nullptr,
            style,
            CRect(0, 0, size_.cx, size_.cy),
            CWnd::FromHandle(taskbar),
            0))
    {
        font_.DeleteObject();
        return false;
    }

    if (!PlaceBesideNotificationArea(taskbar, notificationArea))
    {
        DestroyWindow();
        font_.DeleteObject();
        return false;
    }

    ShowWindow(SW_SHOWNOACTIVATE);
    return true;
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

    size_ = {};
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

bool RateStrip::CreateSystemUiFont(HWND taskbar) noexcept
{
    NONCLIENTMETRICSW metrics{};
    metrics.cbSize = sizeof(metrics);
    if (!SystemParametersInfoW(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0))
    {
        return false;
    }

    const UINT dpi = GetDpiForWindow(taskbar);
    metrics.lfMessageFont.lfHeight = -MulDiv(9, static_cast<int>(dpi), 72);
    return font_.CreateFontIndirectW(&metrics.lfMessageFont) != FALSE;
}

CSize RateStrip::MeasureSize(HWND taskbar) const
{
    const HDC deviceContext = ::GetDC(taskbar);
    if (deviceContext == nullptr)
    {
        return {};
    }

    const HGDIOBJ previousFont = SelectObject(deviceContext, font_.GetSafeHandle());
    SIZE topSize{};
    SIZE bottomSize{};
    TEXTMETRICW textMetrics{};
    const bool measured =
        previousFont != nullptr &&
        GetTextExtentPoint32W(deviceContext, kMaximumTopLine, static_cast<int>(std::size(kMaximumTopLine) - 1), &topSize) != FALSE &&
        GetTextExtentPoint32W(deviceContext, kMaximumBottomLine, static_cast<int>(std::size(kMaximumBottomLine) - 1), &bottomSize) != FALSE &&
        GetTextMetricsW(deviceContext, &textMetrics) != FALSE;

    if (previousFont != nullptr)
    {
        SelectObject(deviceContext, previousFont);
    }
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
    if (!::GetWindowRect(notificationArea, &notificationRect) || !::GetClientRect(taskbar, &taskbarClientRect))
    {
        return false;
    }

    ::MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&notificationRect), 2);

    const UINT dpi = GetDpiForWindow(taskbar);
    const int margin = MulDiv(4, static_cast<int>(dpi), 96);
    const int x = notificationRect.left - size_.cx - margin;
    const int y = (taskbarClientRect.bottom - size_.cy) / 2;
    if (x < taskbarClientRect.left || y < taskbarClientRect.top)
    {
        return false;
    }

    return ::MoveWindow(GetSafeHwnd(), x, y, size_.cx, size_.cy, FALSE) != FALSE;
}

void RateStrip::OnPaint()
{
    CPaintDC deviceContext(this);
    CRect clientRect;
    GetClientRect(&clientRect);

    deviceContext.FillSolidRect(&clientRect, GetSysColor(COLOR_MENU));
    deviceContext.SetBkMode(TRANSPARENT);
    deviceContext.SetTextColor(GetSysColor(COLOR_MENUTEXT));

    CFont* const previousFont = deviceContext.SelectObject(&font_);
    const int midpoint = clientRect.top + clientRect.Height() / 2;
    CRect topLineRect = clientRect;
    topLineRect.bottom = midpoint;
    CRect bottomLineRect = clientRect;
    bottomLineRect.top = midpoint;
    deviceContext.DrawTextW(kTopLine, &topLineRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    deviceContext.DrawTextW(kBottomLine, &bottomLineRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    deviceContext.SelectObject(previousFont);
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
    return HTTRANSPARENT;
}
