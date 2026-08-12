#include "RateStrip.h"

#include <algorithm>
#include <cwchar>
#include <iterator>

namespace
{
constexpr wchar_t kTaskbarClassName[] = L"Shell_TrayWnd";
constexpr wchar_t kNotificationAreaClassName[] = L"TrayNotifyWnd";
constexpr wchar_t kMaximumTopLine[] = L"999.9G/s \u2191";
constexpr wchar_t kMaximumBottomLine[] = L"999.9G/s \u2193";
constexpr wchar_t kMonospaceFont[] = L"Consolas";

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
void RateStrip::SetRates(const std::wstring& uploadText, const std::wstring& downloadText) noexcept
{
    uploadText_ = uploadText;
    downloadText_ = downloadText;
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

bool RateStrip::Relayout(HWND taskbar, HWND notificationArea) noexcept
{
    if (font_.GetSafeHandle() != nullptr)
    {
        font_.DeleteObject();
    }
    if (!CreateSystemUiFont(taskbar))
    {
        return false;
    }
    size_ = MeasureSize(taskbar);
    if (size_.cx <= 0 || size_.cy <= 0)
    {
        return false;
    }
    return PlaceBesideNotificationArea(taskbar, notificationArea);
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
    metrics.lfMessageFont.lfPitchAndFamily = FIXED_PITCH | FF_MODERN;
    wcscpy_s(metrics.lfMessageFont.lfFaceName, kMonospaceFont);
    return font_.CreateFontIndirectW(&metrics.lfMessageFont) != FALSE;
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
    const int x = notificationRect.left - size_.cx - margin;
    const int y = (taskbarClientRect.bottom - size_.cy) / 2;
    if (x < taskbarClientRect.left || y < taskbarClientRect.top)
    {
        return false;
    }

    textColor_ = ChooseTextColor(taskbar, notificationArea);
    return ::SetWindowPos(
               GetSafeHwnd(),
               HWND_TOP,
               x,
               y,
               size_.cx,
               size_.cy,
               SWP_NOACTIVATE | SWP_SHOWWINDOW) != FALSE;
}


COLORREF RateStrip::ChooseTextColor(HWND taskbar, HWND notificationArea) const noexcept
{
    RECT notificationRect{};
    RECT taskbarRect{};
    if (!::GetWindowRect(notificationArea, &notificationRect) ||
        !::GetWindowRect(taskbar, &taskbarRect))
    {
        return GetSysColor(COLOR_MENUTEXT);
    }

    const HDC screen = ::GetDC(nullptr);
    if (screen == nullptr)
    {
        return GetSysColor(COLOR_MENUTEXT);
    }

    const int sampleX = notificationRect.left - size_.cx / 2;
    const int sampleY = taskbarRect.top + (taskbarRect.bottom - taskbarRect.top) / 2;
    const COLORREF background = ::GetPixel(screen, sampleX, sampleY);
    ::ReleaseDC(nullptr, screen);
    if (background == CLR_INVALID)
    {
        return GetSysColor(COLOR_MENUTEXT);
    }

    const int luminance =
        (299 * GetRValue(background) + 587 * GetGValue(background) + 114 * GetBValue(background)) / 1000;
    return luminance >= 140 ? RGB(24, 24, 24) : RGB(255, 255, 255);
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
    const RECT clientRect{ 0, 0, size_.cx, size_.cy };
    ::PatBlt(memoryDc, 0, 0, size_.cx, size_.cy, BLACKNESS);
    ::SetBkMode(memoryDc, TRANSPARENT);
    ::SetTextColor(memoryDc, RGB(255, 255, 255));

    const int midpoint = size_.cy / 2;
    RECT topLineRect = clientRect;
    topLineRect.bottom = midpoint;
    RECT bottomLineRect = clientRect;
    bottomLineRect.top = midpoint;
    const std::wstring topLine = uploadText_ + L" \u2191";
    const std::wstring bottomLine = downloadText_ + L" \u2193";
    ::DrawTextW(memoryDc, topLine.c_str(), -1, &topLineRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);
    ::DrawTextW(memoryDc, bottomLine.c_str(), -1, &bottomLineRect, DT_CENTER | DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX);

    auto* const pixels = static_cast<DWORD*>(bitmapBits);
    const DWORD red = GetRValue(textColor_);
    const DWORD green = GetGValue(textColor_);
    const DWORD blue = GetBValue(textColor_);
    const size_t pixelCount = static_cast<size_t>(size_.cx) * static_cast<size_t>(size_.cy);
    for (size_t index = 0; index < pixelCount; ++index)
    {
        const DWORD mask = pixels[index];
        const DWORD alpha = std::max(GetRValue(mask), std::max(GetGValue(mask), GetBValue(mask)));
        pixels[index] =
            (alpha << 24) |
            ((red * alpha / 255) << 16) |
            ((green * alpha / 255) << 8) |
            (blue * alpha / 255);
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
    return HTTRANSPARENT;
}
