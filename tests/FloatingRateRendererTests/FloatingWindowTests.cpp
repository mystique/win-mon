#include <afxwin.h>
#include "FloatingRateDisplay.h"
#include <iostream>
#include <stdexcept>
#include <dwmapi.h>
#include <fstream>
#include <filesystem>

CWinApp testApp;
int positionNotifications = 0;
LRESULT CALLBACK OwnerProcedure(HWND window,UINT message,WPARAM wParam,LPARAM lParam)
{
    if(message == WM_APP+11) ++positionNotifications;
    return DefWindowProcW(window,message,wParam,lParam);
}
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void CaptureWindow(HWND window, const std::filesystem::path& path)
{
    DwmFlush();
    RECT rect{};
    Require(::GetWindowRect(window, &rect), "capture rectangle");
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    HDC screen = ::GetDC(nullptr);
    HDC memory = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    Require(bitmap != nullptr, "capture bitmap");
    HGDIOBJ old = SelectObject(memory, bitmap);
    const BOOL copied = BitBlt(memory, 0, 0, width, height, screen, rect.left, rect.top, SRCCOPY | CAPTUREBLT);
    BITMAPFILEHEADER header{};
    header.bfType = 0x4d42;
    header.bfOffBits = sizeof(header) + sizeof(info.bmiHeader);
    header.bfSize = header.bfOffBits + width * height * 4;
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&header), sizeof(header));
    output.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
    output.write(static_cast<const char*>(bits), width * height * 4);
    SelectObject(memory, old);
    DeleteObject(bitmap);
    DeleteDC(memory);
    ::ReleaseDC(nullptr, screen);
    Require(copied && output.good(), "native screenshot saved");
}
void Pump(unsigned milliseconds)
{
    const auto end = GetTickCount64() + milliseconds;
    while (GetTickCount64() < end)
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) { TranslateMessage(&message); DispatchMessageW(&message); }
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 8, QS_ALLINPUT);
    }
}
LONG Width(HWND window) { RECT rect{}; Require(::GetWindowRect(window,&rect), "width rectangle"); return rect.right-rect.left; }
int main(int argc, char** argv)
{
    if (!AfxWinInit(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), 0)) return 1;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try
    {
        FloatingRateDisplay display;

        CWnd menuOwner;
        Require(menuOwner.CreateEx(0, AfxRegisterWndClass(0), L"Floating test notifications", WS_POPUP,
            CRect(0, 0, 0, 0), nullptr, 0), "menu notification owner");
        display.SetContextMenuOwner(menuOwner.GetSafeHwnd(), WM_APP + 10);
        SetWindowLongPtrW(menuOwner.GetSafeHwnd(),GWLP_WNDPROC,reinterpret_cast<LONG_PTR>(OwnerProcedure));
        display.SetPositionChangedOwner(menuOwner.GetSafeHwnd(), WM_APP+11);
        const HWND foreground = GetForegroundWindow();
        Require(display.ShowFloating({100, 100}, true), "real floating window initializes");
        const HWND window = display.GetSafeHwnd();
        const HWND shadow = ::GetWindow(window, GW_OWNER);
        Require(shadow != nullptr, "shadow owned separately from interactive body");
        const auto shadowStyle = GetWindowLongPtrW(shadow, GWL_EXSTYLE);
        Require((shadowStyle & (WS_EX_TRANSPARENT | WS_EX_LAYERED)) == (WS_EX_TRANSPARENT | WS_EX_LAYERED),
            "shadow is cross-process input transparent");
        Require(GetForegroundWindow() == foreground, "show does not activate");
        const auto style = GetWindowLongPtrW(window, GWL_EXSTYLE);
        Require((style & (WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) ==
            (WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW), "topmost nonactivating tool window");
        RECT rect{};
        Require(::GetWindowRect(window, &rect), "window rectangle");
        SIZE size = FloatingRateRenderer::SizeForDpi(GetDpiForWindow(window));
        Require(rect.right - rect.left == size.cx && rect.bottom - rect.top == size.cy, "window uses own monitor DPI");
        Require(rect.left == 100 && rect.top == 100, "small saved position retained");
        display.SetRates(L"128.0 K/s", L"8.4 M/s", 128000, 8400000);
        Require(display.Refresh(), "real layered frame submits");
        display.SendMessageW(WM_RBUTTONUP, 0, MAKELPARAM(25, 25));
        MSG notification{};
        Require(PeekMessageW(&notification, menuOwner.GetSafeHwnd(), WM_APP + 10, WM_APP + 10, PM_REMOVE),
            "right click routes to existing Operator Menu owner");
        ::SetWindowPos(window, nullptr, 130, 150, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
        Require(display.GetFloatingPosition().x == 130 && display.GetFloatingPosition().y == 150, "position follows move");
        RECT shadowRect{};
        Require(::GetWindowRect(shadow, &shadowRect) && shadowRect.left == 130 && shadowRect.top == 150, "shadow follows move");
        Require(GetForegroundWindow() == foreground, "move does not activate");
        MONITORINFO monitorInfo{sizeof(monitorInfo)};
        Require(GetMonitorInfoW(MonitorFromWindow(window, MONITOR_DEFAULTTONEAREST), &monitorInfo),
            "drag work area");
        const RECT work = monitorInfo.rcWork;
        const LONG width = size.cx, height = size.cy;
        for (const LONG x : {work.left - 10, work.left + 20, work.right - width + 10})
        {
            for (const LONG y : {work.top - 10, work.top + 20, work.bottom - height + 10})
            {
                RECT proposed{x, y, x + width, y + height};
                display.SendMessageW(WM_MOVING, WMSZ_LEFT, reinterpret_cast<LPARAM>(&proposed));
                Require(proposed.left >= work.left && proposed.top >= work.top &&
                    proposed.right <= work.right && proposed.bottom <= work.bottom,
                    "drag keeps entire window inside all four work area edges");
                Require(proposed.right - proposed.left == width && proposed.bottom - proposed.top == height,
                    "drag constraint preserves window size");
                if (x == work.left + 20 && y == work.top + 20)
                    Require(proposed.left == x && proposed.top == y, "interior drag remains unchanged");
            }
        }
        display.SetRates(L"0.0 K/s", L"0.0 K/s", 0, 0);
        Pump(500);
        const LONG expandedWidth = Width(window);
        Require(display.SetTextVisible(false), "hide text live");
        BOOL animations = FALSE;
        SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION, 0, &animations, 0);
        Pump(55);
        const LONG middleWidth = Width(window);
        if (animations) Require(middleWidth < expandedWidth && middleWidth > FloatingRateRenderer::SizeForDpi(GetDpiForWindow(window),0).cx, "idle transition has intermediate geometry");
        Require(Width(shadow) == middleWidth, "shadow follows intermediate width");
        Require(display.SetTextVisible(true), "reverse transition");
        Require(std::abs(Width(window)-middleWidth) <= 3, "reversal does not jump");
        Pump(250);
        Require(Width(window)==expandedWidth, "reverse completes expansion");
        Require(display.SetTextVisible(false), "collapse again");
        Pump(300);
        Require(Width(window) == FloatingRateRenderer::SizeForDpi(GetDpiForWindow(window),0).cx, "idle collapse completes");
        Require(Width(shadow)==Width(window), "compact shadow size synchronized");
        RECT compact{}; ::GetWindowRect(window,&compact);
        Require(display.SendMessageW(WM_NCHITTEST,0,MAKELPARAM(compact.left+expandedWidth-15,compact.top+size.cy/2))==HTTRANSPARENT, "old text area passes input through");
        Require(display.SetTextVisible(true), "restore visible text");
        Pump(50);
        const LONG beforeFailure = Width(window);
        display.SetRates(L"invalid",L"0.0 K/s",0,0);
        Require(!display.SetTextVisible(false), "invalid readout causes controlled application failure");
        Require(display.IsTextVisible() && Width(window)==beforeFailure, "failed reversal preserves actual geometry and prior target");
        display.SetRates(L"0.0 K/s",L"0.0 K/s",0,0);
        Pump(250);
        Require(Width(window)==expandedWidth, "previous transition resumes after rendering recovers");
        if (argc > 1)
        {
            const std::filesystem::path evidence(argv[1]);
            std::filesystem::create_directories(evidence);
            for (int i = 0; i < 48; ++i)
                display.SetRates(L"128.0 K/s", L"8.4 M/s", 128000, 2000000 + (i % 7) * 1100000);
            display.SetRates(L"128.0 K/s", L"8.4 M/s", 128000, 8400000);
            const ULONGLONG until = GetTickCount64() + 500;
            while (GetTickCount64() < until)
            {
                MSG message{};
                while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
                {
                    TranslateMessage(&message);
                    DispatchMessageW(&message);
                }
                MsgWaitForMultipleObjects(0, nullptr, FALSE, 16, QS_ALLINPUT);
            }
            for (bool light : {false, true})
            {
                HWND backdrop = CreateWindowExW(WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE, L"STATIC", L"", WS_POPUP |
                    (light ? SS_WHITERECT : SS_BLACKRECT), 80, 80, 400, 220, nullptr, nullptr, GetModuleHandleW(nullptr), nullptr);
                Require(backdrop != nullptr, "screenshot backdrop");
                ::SetWindowPos(backdrop, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW);
                ::SetWindowPos(shadow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                ::SetWindowPos(window, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
                ::UpdateWindow(backdrop);
                CaptureWindow(window, evidence / (light ? "native-light.bmp" : "native-dark.bmp"));
                ::DestroyWindow(backdrop);
            }
            display.SetRates(L"0.0 K/s", L"0.0 K/s",0,0);
            Pump(500);
            for (bool visible : {false,true})
            {
                Require(display.SetTextVisible(visible), "native transition evidence");
                for (int frame=0;frame<16;++frame)
                {
                    Pump(16);
                    CaptureWindow(window,evidence / ((visible ? "expand-" : "collapse-") + std::to_string(frame) + ".bmp"));
                }
            }
            display.SetRates(L"1.0 K/s",L"1.0 K/s",1000,1000);
            Pump(500);
            for (int frame=0;frame<30;++frame)
            {
                Pump(33);
                CaptureWindow(window,evidence / ("wave-"+std::to_string(frame)+".bmp"));
            }
            std::cout << "Native screenshot DPI: " << GetDpiForWindow(window) << '\n';
        }
        Require(display.SetTextVisible(false), "compact before edge expansion"); Pump(300);
        ::SetWindowPos(window,nullptr,work.right-Width(window),work.top+20,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
        const int beforeExpansion = positionNotifications;
        Require(display.SetTextVisible(true), "expand at work area edge");
        for(int frame=0;frame<20;++frame)
        {
            Pump(16); RECT edge{}; ::GetWindowRect(window,&edge);
            Require(edge.right<=work.right && edge.left>=work.left, "expansion stays in work area each frame");
        }
        Require(positionNotifications > beforeExpansion, "edge-adjusted position notifies owner after expansion");
        Require(display.SetTextVisible(false), "hide during transition");
        display.ShowWindow(SW_HIDE); Pump(50);
        Require(!::IsWindowVisible(shadow), "whole-window hide also hides shadow");
        Require(display.SetTextVisible(false) && display.ShowFloating({100,100},true), "hidden text state reapplied on show");
        Require(Width(window)==FloatingRateRenderer::SizeForDpi(GetDpiForWindow(window),0).cx, "show opens compact endpoint without animation");
        Require(display.SetTextVisible(true), "restore text"); Pump(250);
        const POINT restored = display.GetFloatingPosition();
        display.Shutdown();
        Require(!IsWindow(window) && !IsWindow(shadow), "shutdown destroys both layers");
        Require(display.SetTextVisible(false), "configure while closed");
        Require(display.ShowFloating(restored, true), "show after hide succeeds");
        Require(Width(display.GetSafeHwnd())==FloatingRateRenderer::SizeForDpi(GetDpiForWindow(display.GetSafeHwnd()),0).cx, "closed configuration opens compact");
        Require(display.GetFloatingPosition().x == restored.x && display.GetFloatingPosition().y == restored.y,
            "reopened position retained");
        display.Shutdown();
        Require(display.ShowFloating({-30000, -30000}, true), "missing monitor recovers");
        Require(MonitorFromWindow(display.GetSafeHwnd(), MONITOR_DEFAULTTONULL) != nullptr, "recovered onto live monitor");
        display.Shutdown();
        const DWORD before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
        for (int i = 0; i < 20; ++i)
        {
            Require(display.ShowFloating({100, 100}, true), "repeat show");
            display.SetRates(L"0.0 K/s", L"0.0 K/s", 0, 0);
            display.Shutdown();
        }
        Require(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before + 1, "no GDI growth after repeated hide/show");
        {
            struct AnimationPreference
            {
                BOOL previous = FALSE;
                AnimationPreference() { Require(SystemParametersInfoW(SPI_GETCLIENTAREAANIMATION,0,&previous,0), "read animation preference"); Require(SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION,0,reinterpret_cast<void*>(FALSE),0), "temporarily disable animations"); }
                ~AnimationPreference() { SystemParametersInfoW(SPI_SETCLIENTAREAANIMATION,0,reinterpret_cast<void*>(static_cast<INT_PTR>(previous)),0); }
            } preference;
            Require(display.SetTextVisible(true) && display.ShowFloating({100,100},true), "static animation mode show");
            display.SetRates(L"1.0 K/s",L"1.0 K/s",1000,1000);
            Require(display.SetTextVisible(false), "static mode hide text");
            Require(Width(display.GetSafeHwnd())==FloatingRateRenderer::SizeForDpi(GetDpiForWindow(display.GetSafeHwnd()),0).cx, "disabled animations jump directly to compact endpoint");
            Require(display.SetChartVisible(false), "hide chart with text hidden");
            const UINT circleDpi = GetDpiForWindow(display.GetSafeHwnd());
            Require(Width(display.GetSafeHwnd()) == MulDiv(54, circleDpi, 96), "both hidden leaves circular body");
            Require(display.SetTextVisible(true), "show text without chart");
            Require(Width(display.GetSafeHwnd()) == FloatingRateRenderer::SizeForDpi(circleDpi,1,0).cx, "text-only geometry");
            Require(display.SetChartVisible(true), "restore charts");
            Require(display.SetTextVisible(true), "static mode show text");
            Require(Width(display.GetSafeHwnd())==FloatingRateRenderer::SizeForDpi(GetDpiForWindow(display.GetSafeHwnd())).cx, "disabled animations jump directly to expanded endpoint");
            display.Shutdown();
        }
        Require(display.SetTextVisible(false) && display.ShowFloating({100,100},true), "show compact for chart transition");
        const int chartWidth = Width(display.GetSafeHwnd());
        Require(display.SetChartVisible(false), "animate charts away"); Pump(70);
        Require(Width(display.GetSafeHwnd()) < chartWidth, "chart transition shrinks live body");
        Require(display.SetChartVisible(true), "reverse chart collapse"); Pump(250);
        Require(Width(display.GetSafeHwnd()) == chartWidth, "chart reversal restores geometry");
        Require(display.SetChartVisible(false), "collapse to circle"); Pump(300);
        Require(Width(display.GetSafeHwnd()) == MulDiv(54,GetDpiForWindow(display.GetSafeHwnd()),96), "animated circle endpoint");
        display.SetRates(L"invalid",L"invalid",0,0);
        Require(!display.SetChartVisible(true) && !display.IsChartVisible(), "failed chart expansion restores preference");
        display.SetRates(L"0.0 K/s",L"0.0 K/s",0,0);
        display.Shutdown();
        menuOwner.DestroyWindow();
        std::cout << "Native floating window checks passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    return 0;
}
