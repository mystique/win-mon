#include "floating/FloatingRateDisplay.h"
#include "taskbar/RateStrip.h"
#include "support/WindowsFixtures.h"

namespace
{
class MenuOwner final : public CWnd
{
public:
    int positionNotifications = 0;
    LRESULT WindowProc(UINT message, WPARAM wparam, LPARAM lparam) override
    {
        if (message == WM_APP + 11)
        {
            ++positionNotifications;
            return 0;
        }
        return CWnd::WindowProc(message, wparam, lparam);
    }
};

class FloatingWindowFixture
{
public:
    FloatingWindowFixture()
    {
        REQUIRE(
            owner.CreateEx(0, AfxRegisterWndClass(0), L"Floating test owner", WS_POPUP, CRect(0, 0, 0, 0), nullptr, 0));
        display.SetContextMenuOwner(owner.GetSafeHwnd(), WM_APP + 10);
        REQUIRE(display.ShowFloating({100, 100}, true));
    }
    ~FloatingWindowFixture()
    {
        display.Shutdown();
    }
    HWND window() const
    {
        return display.GetSafeHwnd();
    }
    HWND shadow() const
    {
        return ::GetWindow(window(), GW_OWNER);
    }
    UINT dpi() const
    {
        return GetDpiForWindow(window());
    }
    void Settle()
    {
        const auto expected = FloatingRateRenderer::SizeForDpi(dpi(), display.IsTextVisible() ? 1.0f : 0.0f,
                                                               display.IsChartVisible() ? 1.0f : 0.0f)
                                  .cx;
        REQUIRE(
            test::PumpUntil([&] { return test::Width(window()) == expected && test::Width(shadow()) == expected; }));
    }
    const HWND foregroundBeforeShow = GetForegroundWindow();
    MenuOwner owner;
    FloatingRateDisplay display;
};
} // namespace

TEST_CASE_METHOD(FloatingWindowFixture, "Floating windows route menus without activation", "[integration][window]")
{
    CHECK(GetForegroundWindow() == foregroundBeforeShow);
    const auto foreground = GetForegroundWindow();
    const auto style = GetWindowLongPtrW(window(), GWL_EXSTYLE);
    CHECK((style & (WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW)) ==
          (WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_TOOLWINDOW));
    REQUIRE(shadow() != nullptr);
    const auto shadowStyle = GetWindowLongPtrW(shadow(), GWL_EXSTYLE);
    CHECK((shadowStyle & (WS_EX_TRANSPARENT | WS_EX_LAYERED)) == (WS_EX_TRANSPARENT | WS_EX_LAYERED));
    CHECK(test::Width(window()) == FloatingRateRenderer::SizeForDpi(dpi()).cx);
    display.SendMessageW(WM_RBUTTONUP, 0, MAKELPARAM(25, 25));
    MSG notification{};
    CHECK(PeekMessageW(&notification, owner.GetSafeHwnd(), WM_APP + 10, WM_APP + 10, PM_REMOVE));
    REQUIRE(::SetWindowPos(window(), nullptr, 130, 150, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    CHECK(display.GetFloatingPosition().x == 130);
    CHECK(display.GetFloatingPosition().y == 150);
    CHECK(test::WindowBounds(shadow()).left == 130);
    CHECK(test::WindowBounds(shadow()).top == 150);
    CHECK(GetForegroundWindow() == foreground);
    CHECK(display.SendMessageW(WM_MOUSEACTIVATE) == MA_NOACTIVATE);
}

TEST_CASE_METHOD(FloatingWindowFixture, "Dragging and expanding keep the body inside the work area",
                 "[integration][window]")
{
    MONITORINFO monitor{sizeof(monitor)};
    REQUIRE(GetMonitorInfoW(MonitorFromWindow(window(), MONITOR_DEFAULTTONEAREST), &monitor));
    const RECT work = monitor.rcWork;
    const auto size = FloatingRateRenderer::SizeForDpi(dpi());
    for (LONG x : {work.left - 10, work.left + 20, work.right - size.cx + 10})
        for (LONG y : {work.top - 10, work.top + 20, work.bottom - size.cy + 10})
        {
            CAPTURE(x, y);
            RECT proposed{x, y, x + size.cx, y + size.cy};
            display.SendMessageW(WM_MOVING, WMSZ_LEFT, reinterpret_cast<LPARAM>(&proposed));
            CHECK(proposed.left >= work.left);
            CHECK(proposed.top >= work.top);
            CHECK(proposed.right <= work.right);
            CHECK(proposed.bottom <= work.bottom);
            CHECK(proposed.right - proposed.left == size.cx);
            CHECK(proposed.bottom - proposed.top == size.cy);
            if (x == work.left + 20 && y == work.top + 20)
            {
                CHECK(proposed.left == x);
                CHECK(proposed.top == y);
            }
        }
    REQUIRE(display.SetTextVisible(false));
    Settle();
    REQUIRE(::SetWindowPos(window(), nullptr, work.right - test::Width(window()), work.top + 20, 0, 0,
                           SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE));
    display.SetPositionChangedOwner(owner.GetSafeHwnd(), WM_APP + 11);
    REQUIRE(display.SetTextVisible(true));
    const auto fullWidth = FloatingRateRenderer::SizeForDpi(dpi()).cx;
    REQUIRE(test::PumpUntil([&] {
        const auto rect = test::WindowBounds(window());
        CHECK(rect.left >= work.left);
        CHECK(rect.right <= work.right);
        return rect.right - rect.left == fullWidth;
    }));
    REQUIRE(test::PumpUntil([&] { return owner.positionNotifications > 0; }));
}

TEST_CASE_METHOD(FloatingWindowFixture, "Visibility changes resize both layers and roll back rendering failures",
                 "[integration][window]")
{
    REQUIRE(display.SetTextVisible(false));
    Settle();
    const auto compact = test::WindowBounds(window());
    const LONG fullWidth = FloatingRateRenderer::SizeForDpi(dpi()).cx;
    CHECK(display.SendMessageW(WM_NCHITTEST, 0, MAKELPARAM(compact.left + fullWidth - 15, compact.top + 27)) ==
          HTTRANSPARENT);
    REQUIRE(display.SetChartVisible(false));
    Settle();
    CHECK(test::Width(window()) == MulDiv(54, dpi(), 96));
    const auto before = test::Width(window());
    display.SetRates(L"invalid", L"invalid", 0, 0);
    CHECK_FALSE(display.SetChartVisible(true));
    CHECK_FALSE(display.IsChartVisible());
    CHECK(test::Width(window()) == before);
    CHECK_FALSE(display.SetTextVisible(true));
    CHECK_FALSE(display.IsTextVisible());
    display.SetRates(L"0.0 K/s", L"0.0 K/s", 0, 0);
    REQUIRE(display.SetTextVisible(true));
    REQUIRE(display.SetChartVisible(true));
    Settle();
    CHECK(test::Width(window()) == fullWidth);
}

TEST_CASE_METHOD(FloatingWindowFixture, "Hidden and recreated windows retain configuration and clean up both layers",
                 "[integration][window]")
{
    REQUIRE(display.SetTextVisible(false));
    display.ShowWindow(SW_HIDE);
    CHECK_FALSE(::IsWindowVisible(shadow()));
    REQUIRE(display.SetTextVisible(false));
    REQUIRE(display.ShowFloating({100, 100}, true));
    Settle();
    const auto body = window(), shadowWindow = shadow();
    const auto restored = display.GetFloatingPosition();
    display.Shutdown();
    CHECK_FALSE(IsWindow(body));
    CHECK_FALSE(IsWindow(shadowWindow));
    REQUIRE(display.ShowFloating(restored, true));
    CHECK_FALSE(display.IsTextVisible());
    CHECK(display.GetFloatingPosition().x == restored.x);
    CHECK(display.GetFloatingPosition().y == restored.y);
    display.Shutdown();
    REQUIRE(display.ShowFloating({-30000, -30000}, true));
    CHECK(MonitorFromWindow(window(), MONITOR_DEFAULTTONULL) != nullptr);
}

TEST_CASE_METHOD(FloatingWindowFixture, "Repeated floating display creation releases GDI objects",
                 "[integration][window][resources]")
{
    display.Shutdown();
    const auto before = GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS);
    for (int i = 0; i < 20; ++i)
    {
        CAPTURE(i);
        REQUIRE(display.ShowFloating({100, 100}, true));
        display.SetRates(L"0.0 K/s", L"0.0 K/s", 0, 0);
        display.Shutdown();
    }
    CHECK(GetGuiResources(GetCurrentProcess(), GR_GDIOBJECTS) <= before + 1);
}

TEST_CASE_METHOD(FloatingWindowFixture, "Double clicks use the visibility handler and preserve failed cycle steps",
                 "[integration][window]")
{
    REQUIRE((GetClassLongPtrW(window(), GCL_STYLE) & CS_DBLCLKS) != 0);
    REQUIRE(display.SetTextVisible(false));
    REQUIRE(display.SetChartVisible(false));
    Settle();
    const int center = MulDiv(27, dpi(), 96);
    const auto click = [&] { display.SendMessageW(WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(center, center)); };
    display.SetVisibilityChangeHandler([](bool, bool) { return false; });
    click();
    CHECK_FALSE(display.IsChartVisible());
    display.SetVisibilityChangeHandler({});
    click();
    CHECK(display.IsChartVisible());
    CHECK_FALSE(display.IsTextVisible());
    click();
    CHECK(display.IsTextVisible());
    click();
    CHECK_FALSE(display.IsTextVisible());
    click();
    CHECK_FALSE(display.IsChartVisible());
    display.SendMessageW(WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(center * 3, center));
    CHECK_FALSE(display.IsChartVisible());
    REQUIRE(display.SetOpacity(35));
    CHECK_FALSE(display.SetOpacity(0));
    CHECK_FALSE(display.SetOpacity(101));
    display.Shutdown();
    CHECK(display.GetOpacity() == 35);
}

TEST_CASE("Unknown shell surfaces cannot be classified as free taskbar space", "[integration][window]")
{
    CWnd unknown;
    REQUIRE(unknown.CreateEx(0, AfxRegisterWndClass(0), L"Unknown shell", WS_POPUP, CRect(0, 0, 200, 60), nullptr, 0));
    RateStrip strip;
    CHECK_FALSE(strip.IsTaskbarSpaceFree(unknown.GetSafeHwnd(), {0, 0, 100, 40}));
}
