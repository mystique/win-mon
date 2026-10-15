// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#include "taskbar/RateStrip.h"
#include "settings/OperatorSettings.h"
#include "floating/LedReadout.h"

#include <algorithm>
#include <array>

#include <initializer_list>
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

struct NativeTaskbarSpace final
{
    RECT candidate;
    HWND xamlHost, strip;
    bool free = true;
};

BOOL CALLBACK CheckNativeTaskbarChild(HWND child, LPARAM parameter)
{
    auto& space = *reinterpret_cast<NativeTaskbarSpace*>(parameter);
    if (child == space.strip || child == space.xamlHost || IsChild(space.xamlHost, child) || IsChild(child, space.xamlHost) ||
        !IsWindowVisible(child)) return TRUE;
    wchar_t className[64]{};
    if (GetClassNameW(child, className, 64) > 0 && wcscmp(className, L"ReBarWindow32") == 0 &&
        FindWindowExW(child, nullptr, L"MSTaskSwWClass", nullptr) != nullptr)
        return TRUE; // The old Win11 container spans empty space; its children are still checked.
    RECT occupied{}, overlap{};
    if (!GetWindowRect(child, &occupied) || IntersectRect(&overlap, &space.candidate, &occupied))
    {
        space.free = false;
        return FALSE;
    }
    return TRUE;
}

}

RateStrip::~RateStrip()
{
    ReleaseSurface();
    d2dBrush_.Reset();
    d2dTarget_.Reset();
    d2dFactory_.Reset();
    automation_.Release();
    if (apartmentOwned_) CoUninitialize();
}

bool RateStrip::IsTaskbarSpaceFree(HWND taskbar, RECT candidate) noexcept
{
    const HWND strip = GetSafeHwnd();
    const HWND bridge = ::FindWindowExW(taskbar, nullptr, L"Windows.UI.Composition.DesktopWindowContentBridge", nullptr);
    const HWND xamlHost = bridge == nullptr ? nullptr :
        ::FindWindowExW(bridge, nullptr, L"Windows.UI.Input.InputSite.WindowClass", nullptr);
    if (xamlHost == nullptr) return false;
    NativeTaskbarSpace native{candidate, xamlHost, strip};
    EnumChildWindows(taskbar, CheckNativeTaskbarChild, reinterpret_cast<LPARAM>(&native));
    if (!native.free) return false;
    if (!automation_)
    {
        if (!apartmentOwned_)
        {
            const HRESULT initialized = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
            apartmentOwned_ = SUCCEEDED(initialized);
            if (FAILED(initialized) && initialized != RPC_E_CHANGED_MODE) return false;
        }
        if (FAILED(automation_.CoCreateInstance(__uuidof(CUIAutomation8)))) return false;
        if (FAILED(automation_->put_ConnectionTimeout(250)) ||
            FAILED(automation_->put_TransactionTimeout(250)))
        {
            automation_.Release();
            return false;
        }
    }
    CComPtr<IUIAutomationElement> root;
    CComPtr<IUIAutomationCondition> condition;
    CComPtr<IUIAutomationCacheRequest> cache;
    CComPtr<IUIAutomationElementArray> controls;
    // Query Explorer's XAML host rather than our own sibling window's UIA provider.
    if (FAILED(automation_->ElementFromHandle(xamlHost, &root)) ||
        FAILED(automation_->get_ControlViewCondition(&condition)) ||
        FAILED(automation_->CreateCacheRequest(&cache)) ||
        FAILED(cache->put_TreeScope(TreeScope_Element)) ||
        FAILED(cache->put_TreeFilter(condition))) return false;
    for (PROPERTYID property : {UIA_NativeWindowHandlePropertyId, UIA_IsOffscreenPropertyId,
        UIA_ClassNamePropertyId, UIA_AutomationIdPropertyId, UIA_BoundingRectanglePropertyId})
    {
        if (FAILED(cache->AddProperty(property))) return false;
    }
    if (FAILED(root->FindAllBuildCache(TreeScope_Descendants, condition, cache, &controls))) return false;
    int count = 0;
    if (FAILED(controls->get_Length(&count))) return false;
    bool foundStart = false;
    for (int index = 0; index < count; ++index)
    {
        CComPtr<IUIAutomationElement> control;
        BOOL offscreen = FALSE;
        UIA_HWND nativeWindow = nullptr;
        CComBSTR className, id;
        if (FAILED(controls->GetElement(index, &control)) ||
            FAILED(control->get_CachedNativeWindowHandle(&nativeWindow)) ||
            FAILED(control->get_CachedIsOffscreen(&offscreen))) return false;
        if (offscreen || (strip != nullptr && reinterpret_cast<HWND>(nativeWindow) == strip)) continue;
        if (FAILED(control->get_CachedClassName(&className)) ||
            FAILED(control->get_CachedAutomationId(&id))) return false;
        const bool structuralFrame = className && wcscmp(className, L"Taskbar.TaskbarFrameAutomationPeer") == 0;
        foundStart |= id && wcscmp(id, L"StartButton") == 0;
        if (structuralFrame) continue;
        RECT occupied{}, overlap{};
        if (FAILED(control->get_CachedBoundingRectangle(&occupied)) || IsRectEmpty(&occupied)) return false;
        if (IntersectRect(&overlap, &candidate, &occupied)) return false;
    }
    return foundStart;
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

void RateStrip::ReleaseSurface() noexcept
{
    if (surfacePrevious_ != nullptr) ::SelectObject(surfaceDc_, surfacePrevious_);
    if (surfaceBitmap_ != nullptr) ::DeleteObject(surfaceBitmap_);
    if (surfaceDc_ != nullptr) ::DeleteDC(surfaceDc_);
    surfaceDc_ = nullptr;
    surfaceBitmap_ = nullptr;
    surfacePrevious_ = nullptr;
    surfaceSize_ = {};
}

void RateStrip::Shutdown() noexcept
{
    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }

    ReleaseSurface();
    d2dBrush_.Reset();
    d2dTarget_.Reset();
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
    const SIZE size = SizeForDpi(GetDpiForWindow(taskbar));
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
    RECT candidate{x, y, x + size_.cx, y + size_.cy};
    ::MapWindowPoints(taskbar, nullptr, reinterpret_cast<POINT*>(&candidate), 2);
    if (taskbarDisplayOnLeft_ && !IsTaskbarSpaceFree(taskbar, candidate))
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

bool RateStrip::RenderToDc(HDC dc, SIZE size, bool lightTheme, bool hitTestableBackground) noexcept
{
    std::array<wchar_t, 9> up{}, down{};
    if (dc == nullptr || size.cx <= 0 || size.cy <= 0 ||
        !led::Cells(uploadText_, L'\u2191', up) || !led::Cells(downloadText_, L'\u2193', down))
    {
        return false;
    }
    if (!d2dFactory_ && FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, d2dFactory_.GetAddressOf())))
    {
        return false;
    }
    if (!d2dTarget_)
    {
        const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
            D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
        if (FAILED(d2dFactory_->CreateDCRenderTarget(&properties, d2dTarget_.GetAddressOf())) ||
            FAILED(d2dTarget_->CreateSolidColorBrush(D2D1::ColorF(0xffffff), d2dBrush_.GetAddressOf())))
        {
            d2dBrush_.Reset();
            d2dTarget_.Reset();
            return false;
        }
    }
    const RECT bounds{0, 0, size.cx, size.cy};
    if (FAILED(d2dTarget_->BindDC(dc, &bounds))) return false;
    d2dTarget_->BeginDraw();
    d2dTarget_->Clear(D2D1::ColorF(0, hitTestableBackground ? (1.0f / 255.0f) : 0.0f));
    d2dTarget_->SetTransform(D2D1::Matrix3x2F::Scale(size.cx / 55.0f, size.cy / 22.0f));
    led::Draw(d2dTarget_.Get(), d2dBrush_.Get(), up, up, 2, 8, 0x61e5b7, 1, 1, lightTheme, !taskbarDisplayOnLeft_, true);
    led::Draw(d2dTarget_.Get(), d2dBrush_.Get(), down, down, 2, 19, 0x32bdeb, 1, 1, lightTheme, !taskbarDisplayOnLeft_, true);
    const HRESULT hr = d2dTarget_->EndDraw();
    if (hr == D2DERR_RECREATE_TARGET)
    {
        d2dBrush_.Reset();
        d2dTarget_.Reset();
    }
    return SUCCEEDED(hr);
}

bool RateStrip::Render() noexcept
{
    if (GetSafeHwnd() == nullptr || size_.cx <= 0 || size_.cy <= 0)
    {
        return false;
    }

    if (surfaceDc_ == nullptr || surfaceSize_.cx != size_.cx || surfaceSize_.cy != size_.cy)
    {
        ReleaseSurface();
        surfaceDc_ = ::CreateCompatibleDC(nullptr);
        if (surfaceDc_ == nullptr) return false;
        BITMAPINFO bitmapInfo{};
        bitmapInfo.bmiHeader = {sizeof(BITMAPINFOHEADER), size_.cx, -size_.cy, 1, 32, BI_RGB};
        void* bitmapBits = nullptr;
        surfaceBitmap_ = ::CreateDIBSection(surfaceDc_, &bitmapInfo, DIB_RGB_COLORS, &bitmapBits, nullptr, 0);
        if (surfaceBitmap_ == nullptr || bitmapBits == nullptr)
        {
            ReleaseSurface();
            return false;
        }
        surfacePrevious_ = ::SelectObject(surfaceDc_, surfaceBitmap_);
        if (surfacePrevious_ == nullptr || surfacePrevious_ == HGDI_ERROR)
        {
            surfacePrevious_ = nullptr;
            ReleaseSurface();
            return false;
        }
        surfaceSize_ = {size_.cx, size_.cy};
    }
    POINT source{};
    SIZE windowSize{size_.cx, size_.cy};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    return RenderToDc(surfaceDc_, windowSize, settings::IsLightTheme(), contextMenuEnabled_) &&
        ::UpdateLayeredWindow(GetSafeHwnd(), nullptr, nullptr, &windowSize, surfaceDc_, &source, 0, &blend, ULW_ALPHA) != FALSE;
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
