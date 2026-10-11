#include <afxwin.h>
#include "taskbar/RateStrip.h"
#include "taskbar/TaskbarSpaceProbe.h"
#include "settings/SpeedTextPositionSettingChange.h"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <filesystem>
#include <fstream>
#include <dwmapi.h>

CWinApp testApp;
bool canPersist = false, savedLeft = false;
bool PersistPosition(bool left)
{
    if (canPersist) savedLeft = left;
    return canPersist;
}
void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void Capture(HWND window, const std::filesystem::path& path)
{
    DwmFlush();
    RECT rect{};
    Require(::GetWindowRect(window, &rect), "capture bounds");
    const int width = rect.right - rect.left, height = rect.bottom - rect.top;
    HDC screen = ::GetDC(nullptr), memory = CreateCompatibleDC(screen);
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), width, -height, 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    Require(bitmap != nullptr, "capture bitmap");
    auto old = SelectObject(memory, bitmap);
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
        RateStrip strip;
        Require(strip.SetSpeedTextOnLeft(true) && strip.Embed(true), "Left creates a native strip");
        const HWND taskbar = ::FindWindowW(L"Shell_TrayWnd", nullptr);
        const bool initiallyVisible = strip.IsDisplayVisible();
        std::filesystem::path evidence;
        if (argc > 1) { evidence = argv[1]; std::filesystem::create_directories(evidence); }
        Require(strip.Refresh(), "Left refreshes even when unavailable");
        if (initiallyVisible)
        {
            CComPtr<IUIAutomation> automation;
            CComPtr<IUIAutomationElement> root, start;
            CComPtr<IUIAutomationCondition> condition;
            const HWND bridge = FindWindowExW(taskbar, nullptr, L"Windows.UI.Composition.DesktopWindowContentBridge", nullptr);
            const HWND host = FindWindowExW(bridge, nullptr, L"Windows.UI.Input.InputSite.WindowClass", nullptr);
            Require(SUCCEEDED(automation.CoCreateInstance(__uuidof(CUIAutomation8))) &&
                SUCCEEDED(automation->ElementFromHandle(host, &root)) &&
                SUCCEEDED(automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, CComVariant(L"StartButton"), &condition)) &&
                SUCCEEDED(root->FindFirst(TreeScope_Descendants, condition, &start)) && start, "real XAML Start control");
            RECT startBounds{};
            Require(SUCCEEDED(start->get_CurrentBoundingRectangle(&startBounds)), "Start clickable bounds");
            RECT startPadding{startBounds.left + 1, startBounds.top + 1, startBounds.left + 2, startBounds.top + 2};
            MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&startPadding), 2);
            Require(!TaskbarSpaceProbe{}.IsFree(taskbar, startPadding, strip.GetSafeHwnd()), "Start clickable padding is occupied even outside icon content");
            RECT rect{}, client{};
            Require(::GetWindowRect(strip.GetSafeHwnd(), &rect), "Left bounds");
            MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&rect), 2);
            Require(rect.left == MulDiv(4, GetDpiForWindow(taskbar), 96), "Left anchors at 4 DIP");
            const HWND foreground = GetForegroundWindow();
            strip.SetRates(L"1000.0 M/s", L"0.0 K/s");
            Require(strip.Refresh() && strip.IsDisplayVisible(), "Left remains visible with different readings");
            if (!evidence.empty()) Capture(strip.GetSafeHwnd(), evidence / "left.bmp");
            Require(GetForegroundWindow() == foreground, "strip does not activate");
            // A temporary test child models a newly occupied shell region without moving shell controls.
            CWnd obstacle;
            Require(obstacle.CreateEx(0, L"BUTTON", L"Test taskbar occupancy", WS_CHILD | WS_VISIBLE,
                CRect(rect), CWnd::FromHandle(taskbar), 0), "temporary occupied region");
            Require(strip.Refresh() && !strip.IsDisplayVisible() && !strip.IsWindowVisible(), "occupied Left hides");
            obstacle.DestroyWindow();
            Require(strip.Refresh() && strip.IsDisplayVisible(), "Left returns when region is free");
            Require(::GetClientRect(taskbar, &client), "taskbar geometry stays available");
        }
        else std::cout << "Left region is unavailable on this desktop; visible Left checks skipped\n";
        CWnd unknown;
        Require(unknown.CreateEx(0, AfxRegisterWndClass(0), L"Unknown taskbar", WS_POPUP,
            CRect(0, 0, 200, 60), nullptr, 0), "unknown surface");
        Require(!TaskbarSpaceProbe{}.IsFree(unknown.GetSafeHwnd(), {0, 0, 100, 40}), "unknown shell cannot prove free space");
        Require(strip.SetSpeedTextOnLeft(false) && strip.IsDisplayVisible(), "switch back to Right");
        if (!evidence.empty()) Capture(strip.GetSafeHwnd(), evidence / "right.bmp");
        Require(!strip.IsSpeedTextOnLeft(), "Right selection retained");
        SpeedTextPositionSettingChange change{strip, true, PersistPosition};
        Require(winmon::OperatorSettingTransaction::Commit(change) == winmon::OperatorSettingOutcome::RolledBack &&
            !strip.IsSpeedTextOnLeft() && !savedLeft, "failed persistence restores native Right layout");
        canPersist = true;
        Require(winmon::OperatorSettingTransaction::Commit(change) == winmon::OperatorSettingOutcome::Applied &&
            strip.IsSpeedTextOnLeft() && savedLeft, "position transaction applies and saves Left");
        const auto started = std::chrono::steady_clock::now();
        for (int sample = 0; sample < 3; ++sample) Require(strip.Refresh(), "steady Left refresh");
        std::cout << "Three Left refreshes took " << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count() << " ms\n";
        Require(strip.SetSpeedTextOnLeft(false), "restore Right before recreation");
        strip.Shutdown();
        Require(strip.Embed(true) && !strip.IsSpeedTextOnLeft(), "Right survives surface recreation");
        Require(strip.SetSpeedTextOnLeft(true), "return to Left");
        strip.Shutdown();
        Require(strip.Embed(true) && strip.IsSpeedTextOnLeft(), "Left survives surface recreation");
        RateFontSelection font;
        Require(strip.GetRateFont(font), "active font");
        wcscpy_s(font.logFont.lfFaceName, L"Arial");
        font.pointSizeTenths = 120;
        font.logFont.lfItalic = TRUE;
        Require(strip.SetRateFont(font) && strip.Refresh(), "font changes rebuild selected layout");
        strip.Shutdown();
        std::cout << "RateStripWindowTests passed; visible Left verified=" << initiallyVisible << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
