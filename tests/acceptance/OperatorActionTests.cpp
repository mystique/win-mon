#include "app/TrayMessageWindow.h"
#include "settings/OperatorSettings.h"
#include "support/IsolatedRegistry.h"
#include "support/WindowsFixtures.h"
#include <shellapi.h>

namespace
{
class OperatorFixture
{
public:
    OperatorFixture()
    {
        REQUIRE(settings::SetTaskbarTextVisible(false));
        REQUIRE(settings::SetTrayIconVisible(true));
        REQUIRE(settings::SetFloatingDisplayEnabled(true));
        if (!host.Initialize())
        {
            host.Shutdown();
            FAIL("Desktop host initialization failed");
        }
    }
    ~OperatorFixture()
    {
        host.Shutdown();
        test::DispatchMessages();
    }
    HWND body() const
    {
        HWND found = nullptr;
        EnumWindows(
            [](HWND window, LPARAM parameter) -> BOOL {
                DWORD process = 0;
                GetWindowThreadProcessId(window, &process);
                const auto style = GetWindowLongPtrW(window, GWL_EXSTYLE);
                if (process == GetCurrentProcessId() && (style & WS_EX_LAYERED) && !(style & WS_EX_TRANSPARENT) &&
                    ::GetWindow(window, GW_OWNER))
                {
                    *reinterpret_cast<HWND*>(parameter) = window;
                    return FALSE;
                }
                return TRUE;
            },
            reinterpret_cast<LPARAM>(&found));
        REQUIRE(found != nullptr);
        return found;
    }
    bool HasTrayIcon() const
    {
        NOTIFYICONIDENTIFIER icon{sizeof(icon)};
        icon.hWnd = host.GetSafeHwnd();
        icon.uID = 1;
        RECT bounds{};
        return SUCCEEDED(Shell_NotifyIconGetRect(&icon, &bounds));
    }
    test::IsolatedRegistry registry;
    TrayMessageWindow host;
};
} // namespace

TEST_CASE_METHOD(OperatorFixture, "Floating menu commands roll back actual windows when persistence fails",
                 "[acceptance][settings]")
{
    const auto window = body();
    const auto shadow = ::GetWindow(window, GW_OWNER);
    const auto before = test::Width(window);
    test::RegistryWriteBlock deny(L"Software\\Win Mon");
    host.ExecuteOperatorAction({winmon::OperatorActionKind::HideFloatingText});
    CHECK(host.MenuToggles().floatingTextVisible);
    CHECK(settings::IsFloatingTextVisible());
    REQUIRE(test::PumpUntil([&] { return test::Width(window) == before && test::Width(shadow) == before; }));
    host.ExecuteOperatorAction({winmon::OperatorActionKind::HideFloatingCharts});
    CHECK(host.MenuToggles().floatingChartsVisible);
    CHECK(settings::AreFloatingChartsVisible());
    host.ExecuteOperatorAction({winmon::OperatorActionKind::SetFloatingDisplayOpacity, 40});
    CHECK(host.MenuToggles().floatingDisplayOpacity == 100);
    CHECK(settings::GetFloatingDisplayOpacity() == 100);
}

TEST_CASE_METHOD(OperatorFixture, "Floating double clicks persist through the real command handler",
                 "[acceptance][settings]")
{
    const auto window = body();
    const int center = MulDiv(27, GetDpiForWindow(window), 96);
    {
        test::RegistryWriteBlock deny(L"Software\\Win Mon");
        ::SendMessageW(window, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(center, center));
        CHECK(host.MenuToggles().floatingTextVisible);
        CHECK(host.MenuToggles().floatingChartsVisible);
    }
    ::SendMessageW(window, WM_LBUTTONDBLCLK, MK_LBUTTON, MAKELPARAM(center, center));
    CHECK_FALSE(host.MenuToggles().floatingTextVisible);
    CHECK_FALSE(settings::IsFloatingTextVisible());
    CHECK(host.MenuToggles().floatingChartsVisible);
}

TEST_CASE_METHOD(OperatorFixture, "Temporary tray fallback preserves the saved hidden preference",
                 "[acceptance][access]")
{
    host.ExecuteOperatorAction({winmon::OperatorActionKind::HideTrayIcon});
    REQUIRE_FALSE(settings::IsTrayIconVisible());
    REQUIRE_FALSE(HasTrayIcon());
    ::ShowWindow(body(), SW_HIDE);
    host.SendMessageW(WM_THEMECHANGED);
    CHECK(HasTrayIcon());
    CHECK_FALSE(settings::IsTrayIconVisible());
    CHECK_FALSE(host.MenuToggles().trayIconVisible);
    host.ExecuteOperatorAction({winmon::OperatorActionKind::ShowFloatingDisplay});
    CHECK(host.MenuToggles().floatingDisplayAvailable);
    CHECK_FALSE(HasTrayIcon());
    CHECK_FALSE(settings::IsTrayIconVisible());
}

TEST_CASE_METHOD(OperatorFixture, "Commands reject closing the last currently available menu entry",
                 "[acceptance][access]")
{
    host.ExecuteOperatorAction({winmon::OperatorActionKind::HideTrayIcon});
    host.ExecuteOperatorAction({winmon::OperatorActionKind::HideFloatingDisplay});
    CHECK(host.MenuToggles().floatingDisplayAvailable);
    CHECK(settings::IsFloatingDisplayEnabled());
}
