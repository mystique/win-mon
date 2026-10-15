#include <afxwin.h>
#include "taskbar/RateStrip.h"
#include "taskbar/TaskbarSpaceProbe.h"
#include "settings/TaskbarDisplayPositionSettingChange.h"
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

constexpr UINT kMenuMessage = WM_APP + 20;
class MenuReceiver final : public CWnd
{
public:
    int requests = 0;
    LRESULT WindowProc(UINT message, WPARAM wparam, LPARAM lparam) override
    {
        if (message == kMenuMessage) { ++requests; return 0; }
        return CWnd::WindowProc(message, wparam, lparam);
    }
};

void CheckPlacementAndInput(RateStrip& strip, HWND taskbar, MenuReceiver& receiver)
{
    RECT bounds{}, client{};
    Require(::GetWindowRect(strip.GetSafeHwnd(), &bounds) && ::GetClientRect(taskbar, &client), "placement geometry");
    const POINT center{(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
    MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&bounds), 2);
    Require(bounds.top == (client.bottom - (bounds.bottom - bounds.top)) / 2, "strip stays vertically centered");
    const LONG style = GetWindowLongW(strip.GetSafeHwnd(), GWL_STYLE);
    Require((style & WS_CHILD) != 0 && (style & WS_POPUP) == 0, "embedded strip uses child window coordinates and input routing");
    Require(WindowFromPoint(center) == strip.GetSafeHwnd(), "visible interactive strip receives real mouse hit tests");
    POINT previousCursor{};
    Require(GetCursorPos(&previousCursor) && SetCursorPos(center.x, center.y), "position test pointer");
    const int previousRequests = receiver.requests;
    INPUT clicks[2]{};
    clicks[0].type = clicks[1].type = INPUT_MOUSE;
    clicks[0].mi.dwFlags = MOUSEEVENTF_RIGHTDOWN;
    clicks[1].mi.dwFlags = MOUSEEVENTF_RIGHTUP;
    const UINT sent = SendInput(2, clicks, sizeof(INPUT));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(500);
    while (receiver.requests == previousRequests && std::chrono::steady_clock::now() < deadline)
    {
        MSG message{};
        while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&message);
            DispatchMessageW(&message);
        }
        Sleep(1);
    }
    SetCursorPos(previousCursor.x, previousCursor.y);
    Require(sent == 2 && receiver.requests == previousRequests + 1, "real right click requests the Operator Menu");
    strip.SetContextMenuEnabled(false);
    Require(WindowFromPoint(center) != strip.GetSafeHwnd(), "disabled strip passes mouse input through");
    strip.SetContextMenuEnabled(true);
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
        MenuReceiver receiver;
        Require(receiver.CreateEx(0, AfxRegisterWndClass(0), L"Test menu receiver", WS_POPUP,
            CRect(0, 0, 0, 0), nullptr, 0), "menu receiver");
        strip.SetContextMenuOwner(receiver.GetSafeHwnd(), kMenuMessage);
        strip.SetContextMenuEnabled(true);
        Require(strip.Embed(true), "Right creates a native strip");
        Require(strip.SetTaskbarDisplayOnLeft(true), "switch from Right to Left");
        const HWND taskbar = ::FindWindowW(L"Shell_TrayWnd", nullptr);
        const bool initiallyVisible = strip.IsDisplayVisible();
        if (argc > 2 && std::string(argv[2]) == "--require-left")
            Require(initiallyVisible, "unoccupied Left must remain visible after switching from Right");
        std::filesystem::path evidence;
        if (argc > 1) { evidence = argv[1]; std::filesystem::create_directories(evidence); }
        Require(strip.Refresh(), "Left refreshes even when unavailable");
        if (initiallyVisible)
        {
            CheckPlacementAndInput(strip, taskbar, receiver);
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
        Require(strip.SetTaskbarDisplayOnLeft(false) && strip.IsDisplayVisible(), "switch back to Right");
        strip.SetRates(L"123.4 K/s", L"56.7 M/s");
        if (!evidence.empty())
        {
            Capture(strip.GetSafeHwnd(), evidence / "right.bmp");
            Capture(taskbar, evidence / "taskbar.bmp");
            strip.SetRates(L"1.8 K/s", L"1.8 K/s");
            Capture(strip.GetSafeHwnd(), evidence / "quiet.bmp");
        }
        Require(!strip.IsTaskbarDisplayOnLeft(), "Right selection retained");
        TaskbarDisplayPositionSettingChange change{strip, true, PersistPosition};
        Require(winmon::OperatorSettingTransaction::Commit(change) == winmon::OperatorSettingOutcome::RolledBack &&
            !strip.IsTaskbarDisplayOnLeft() && !savedLeft, "failed persistence restores native Right layout");
        canPersist = true;
        Require(winmon::OperatorSettingTransaction::Commit(change) == winmon::OperatorSettingOutcome::Applied &&
            strip.IsTaskbarDisplayOnLeft() && savedLeft, "position transaction applies and saves Left");
        const auto started = std::chrono::steady_clock::now();
        for (int sample = 0; sample < 3; ++sample) Require(strip.Refresh(), "steady Left refresh");
        std::cout << "Three Left refreshes took " << std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - started).count() << " ms\n";
        Require(strip.SetTaskbarDisplayOnLeft(false), "restore Right before recreation");
        strip.Shutdown();
        Require(strip.Embed(true) && !strip.IsTaskbarDisplayOnLeft(), "Right survives surface recreation");
        Require(strip.SetTaskbarDisplayOnLeft(true), "return to Left");
        strip.Shutdown();
        Require(strip.Embed(true) && strip.IsTaskbarDisplayOnLeft(), "Left survives surface recreation");
        if (initiallyVisible) CheckPlacementAndInput(strip, taskbar, receiver);
        strip.Shutdown();
        std::cout << "RateStripWindowTests passed; visible Left verified=" << initiallyVisible << '\n';
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
