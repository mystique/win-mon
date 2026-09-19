#include <afxwin.h>
#include "RateStrip.h"
#include <iostream>
#include <stdexcept>
#include <dwmapi.h>
#include <fstream>
#include <filesystem>

CWinApp testApp;
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
int main(int argc, char** argv)
{
    if (!AfxWinInit(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), 0)) return 1;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    try
    {
        RateStrip display;
        display.SetContextMenuEnabled(true);
        CWnd menuOwner;
        Require(menuOwner.CreateEx(0, AfxRegisterWndClass(0), L"Floating test notifications", WS_POPUP,
            CRect(0, 0, 0, 0), nullptr, 0), "menu notification owner");
        display.SetContextMenuOwner(menuOwner.GetSafeHwnd(), WM_APP + 10);
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
        if (argc > 1)
        {
            const std::filesystem::path evidence(argv[1]);
            std::filesystem::create_directories(evidence);
            for (int i = 0; i < 48; ++i)
                display.SetRates(L"128.0 K/s", L"8.4 M/s", 128000, 2000000 + (i % 7) * 1100000);
            display.SetRates(L"128.0 K/s", L"8.4 M/s", 128000, 8400000);
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
            std::cout << "Native screenshot DPI: " << GetDpiForWindow(window) << '\n';
        }
        const POINT restored = display.GetFloatingPosition();
        display.Shutdown();
        Require(!IsWindow(window) && !IsWindow(shadow), "shutdown destroys both layers");
        Require(display.ShowFloating(restored, true), "show after hide succeeds");
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
