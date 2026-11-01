#include "support/WindowsFixtures.h"
#include "taskbar/RateStrip.h"
#include <catch2/catch_test_macros.hpp>
#include <chrono>

constexpr UINT kMenuMessage = WM_APP + 20;
class MenuReceiver final : public CWnd
{
public:
    int requests = 0;
    LRESULT WindowProc(UINT message, WPARAM wparam, LPARAM lparam) override
    {
        if (message == kMenuMessage)
        {
            ++requests;
            return 0;
        }
        return CWnd::WindowProc(message, wparam, lparam);
    }
};

void CheckPlacementAndInput(RateStrip& strip, HWND taskbar, MenuReceiver& receiver)
{
    RECT bounds{}, client{};
    REQUIRE((::GetWindowRect(strip.GetSafeHwnd(), &bounds) && ::GetClientRect(taskbar, &client)));
    const POINT center{(bounds.left + bounds.right) / 2, (bounds.top + bounds.bottom) / 2};
    MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&bounds), 2);
    REQUIRE(bounds.top == (client.bottom - (bounds.bottom - bounds.top)) / 2);
    const LONG style = GetWindowLongW(strip.GetSafeHwnd(), GWL_STYLE);
    REQUIRE(((style & WS_CHILD) != 0 && (style & WS_POPUP) == 0));
    REQUIRE(WindowFromPoint(center) == strip.GetSafeHwnd());
    test::CursorRestore cursor;
    REQUIRE(cursor.saved);
    REQUIRE(SetCursorPos(center.x, center.y));
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
    REQUIRE((sent == 2 && receiver.requests == previousRequests + 1));
    strip.SetContextMenuEnabled(false);
    REQUIRE(WindowFromPoint(center) != strip.GetSafeHwnd());
    strip.SetContextMenuEnabled(true);
}

TEST_CASE("Visible Left handles real input and occupied regions", "[acceptance][taskbar]")
{
    RateStrip strip;
    MenuReceiver receiver;
    REQUIRE(
        receiver.CreateEx(0, AfxRegisterWndClass(0), L"Test menu receiver", WS_POPUP, CRect(0, 0, 0, 0), nullptr, 0));
    strip.SetContextMenuOwner(receiver.GetSafeHwnd(), kMenuMessage);
    strip.SetContextMenuEnabled(true);
    REQUIRE(strip.Embed(true));
    REQUIRE(strip.SetTaskbarDisplayOnLeft(true));
    const HWND taskbar = ::FindWindowW(L"Shell_TrayWnd", nullptr);
    if (!strip.IsDisplayVisible())
        SKIP("Left region is occupied or cannot be verified on this desktop");

    CheckPlacementAndInput(strip, taskbar, receiver);
    CComPtr<IUIAutomation> automation;
    CComPtr<IUIAutomationElement> root, start;
    CComPtr<IUIAutomationCondition> condition;
    const HWND bridge = FindWindowExW(taskbar, nullptr, L"Windows.UI.Composition.DesktopWindowContentBridge", nullptr);
    const HWND host = FindWindowExW(bridge, nullptr, L"Windows.UI.Input.InputSite.WindowClass", nullptr);
    REQUIRE((SUCCEEDED(automation.CoCreateInstance(__uuidof(CUIAutomation8))) &&
             SUCCEEDED(automation->ElementFromHandle(host, &root)) &&
             SUCCEEDED(automation->CreatePropertyCondition(UIA_AutomationIdPropertyId, CComVariant(L"StartButton"),
                                                           &condition)) &&
             SUCCEEDED(root->FindFirst(TreeScope_Descendants, condition, &start)) && start));
    RECT startBounds{};
    REQUIRE(SUCCEEDED(start->get_CurrentBoundingRectangle(&startBounds)));
    RECT startPadding{startBounds.left + 1, startBounds.top + 1, startBounds.left + 2, startBounds.top + 2};
    REQUIRE(!strip.IsTaskbarSpaceFree(taskbar, startPadding));
    RECT rect{}, client{};
    REQUIRE(::GetWindowRect(strip.GetSafeHwnd(), &rect));
    MapWindowPoints(nullptr, taskbar, reinterpret_cast<POINT*>(&rect), 2);
    REQUIRE(rect.left == MulDiv(4, GetDpiForWindow(taskbar), 96));
    const HWND foreground = GetForegroundWindow();
    strip.SetRates(L"1000.0 M/s", L"0.0 K/s");
    REQUIRE((strip.Refresh() && strip.IsDisplayVisible()));
    REQUIRE(GetForegroundWindow() == foreground);
    // A temporary test child models a newly occupied shell region without moving shell controls.
    CWnd obstacle;
    REQUIRE(obstacle.CreateEx(0, L"BUTTON", L"Test taskbar occupancy", WS_CHILD | WS_VISIBLE, CRect(rect),
                              CWnd::FromHandle(taskbar), 0));
    REQUIRE((strip.Refresh() && !strip.IsDisplayVisible() && !strip.IsWindowVisible()));
    obstacle.DestroyWindow();
    REQUIRE((strip.Refresh() && strip.IsDisplayVisible()));
    REQUIRE(::GetClientRect(taskbar, &client));

    strip.Shutdown();
}
