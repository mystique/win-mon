#include "settings/OperatorSettings.h"
#include "support/IsolatedRegistry.h"
#include <catch2/generators/catch_generators.hpp>

namespace
{
constexpr auto productKey = L"Software\\Win Mon";
struct BooleanSetting
{
    const wchar_t* name;
    bool defaultValue;
    bool (*read)() noexcept;
    bool (*write)(bool) noexcept;
};
const BooleanSetting flags[]{
    {L"TaskbarRatesVisible", true, settings::IsTaskbarRatesVisible, settings::SetTaskbarRatesVisible},
    {L"TaskbarMemoryVisible", true, settings::IsTaskbarMemoryVisible, settings::SetTaskbarMemoryVisible},
    {L"TaskbarTextVisible", true, settings::IsTaskbarTextVisible, settings::SetTaskbarTextVisible},
    {L"TrayIconVisible", true, settings::IsTrayIconVisible, settings::SetTrayIconVisible},
    {L"TaskbarContextMenuEnabled", false, settings::IsTaskbarContextMenuEnabled,
     settings::SetTaskbarContextMenuEnabled},
    {L"FloatingDisplayVisible", false, settings::IsFloatingDisplayEnabled, settings::SetFloatingDisplayEnabled},
    {L"FloatingTextVisible", true, settings::IsFloatingTextVisible, settings::SetFloatingTextVisible},
    {L"FloatingChartsVisible", true, settings::AreFloatingChartsVisible, settings::SetFloatingChartsVisible},
    {L"TaskbarDisplayPosition", false, settings::IsTaskbarDisplayOnLeft, settings::SetTaskbarDisplayOnLeft}};
} // namespace

TEST_CASE("Boolean settings persist and reject invalid registry data", "[integration][registry]")
{
    const auto index = GENERATE(0, 1, 2, 3, 4, 5, 6, 7, 8);
    CAPTURE(index);
    test::IsolatedRegistry registry;
    const auto& setting = flags[index];
    CHECK(setting.read() == setting.defaultValue);
    for (bool value : {false, true})
    {
        CAPTURE(value);
        REQUIRE(setting.write(value));
        CHECK(setting.read() == value);
    }
    for (DWORD value : {2u, 0xffffffffu})
    {
        test::WriteDword(setting.name, value);
        CHECK(setting.read() == setting.defaultValue);
    }
    const wchar_t text[] = L"1";
    test::WriteValue(productKey, setting.name, REG_SZ, text, sizeof(text));
    CHECK(setting.read() == setting.defaultValue);
    const BYTE shortValue = 1;
    test::WriteValue(productKey, setting.name, REG_DWORD, &shortValue, sizeof(shortValue));
    CHECK(setting.read() == setting.defaultValue);
}

TEST_CASE("Startup repairs only configurations with no menu entry", "[integration][registry][access]")
{
    test::IsolatedRegistry registry;
    for (unsigned mask = 0; mask < 16; ++mask)
    {
        CAPTURE(mask);
        const bool tray = (mask & 1) != 0, floating = (mask & 2) != 0;
        const bool text = (mask & 4) != 0, context = (mask & 8) != 0;
        REQUIRE(settings::SetTrayIconVisible(tray));
        REQUIRE(settings::SetFloatingDisplayEnabled(floating));
        REQUIRE(settings::SetTaskbarTextVisible(text));
        REQUIRE(settings::SetTaskbarContextMenuEnabled(context));
        const bool repair = !tray && !floating && !(text && context);
        const auto toggles = settings::LoadStartupToggles();
        CHECK(toggles.trayIconVisible == (tray || repair));
        CHECK(toggles.floatingDisplayEnabled == (floating || repair));
        CHECK(toggles.taskbarTextVisible == text);
        CHECK(toggles.taskbarContextMenuEnabled == context);
        CHECK(settings::IsTrayIconVisible() == toggles.trayIconVisible);
        CHECK(settings::IsFloatingDisplayEnabled() == toggles.floatingDisplayEnabled);
    }
}

TEST_CASE("Startup retains live recovery choices when registry writes are denied", "[integration][registry][access]")
{
    test::IsolatedRegistry registry;
    REQUIRE(settings::SetTrayIconVisible(false));
    REQUIRE(settings::SetFloatingDisplayEnabled(false));
    REQUIRE(settings::SetTaskbarTextVisible(false));
    test::RegistryWriteBlock deny(productKey);
    REQUIRE_FALSE(settings::SetTrayIconVisible(true));
    const auto toggles = settings::LoadStartupToggles();
    CHECK(toggles.trayIconVisible);
    CHECK(toggles.floatingDisplayEnabled);
    CHECK_FALSE(toggles.taskbarTextVisible);
    CHECK_FALSE(settings::IsTrayIconVisible());
    CHECK_FALSE(settings::IsFloatingDisplayEnabled());
}

TEST_CASE("Floating position validates binary type and length", "[integration][registry]")
{
    test::IsolatedRegistry registry;
    POINT position{};
    CHECK_FALSE(settings::GetFloatingDisplayPosition(position));
    REQUIRE(settings::SetFloatingDisplayPosition({-120, 240}));
    REQUIRE(settings::GetFloatingDisplayPosition(position));
    CHECK(position.x == -120);
    CHECK(position.y == 240);
    const DWORD invalid = 1;
    for (DWORD type : {REG_DWORD, REG_BINARY})
    {
        test::WriteValue(productKey, L"FloatingDisplayPosition", type, &invalid, sizeof(invalid));
        CHECK_FALSE(settings::GetFloatingDisplayPosition(position));
    }
}

TEST_CASE("Opacity validates writes and normalizes legacy values", "[integration][registry]")
{
    test::IsolatedRegistry registry;
    CHECK(settings::GetFloatingDisplayOpacity() == 100);
    for (int value : {10, 40, 100})
    {
        REQUIRE(settings::SetFloatingDisplayOpacity(value));
        CHECK(settings::GetFloatingDisplayOpacity() == value);
    }
    for (int value : {0, 101, 45})
        CHECK_FALSE(settings::SetFloatingDisplayOpacity(value));
    test::WriteDword(L"FloatingDisplayOpacity", 45);
    CHECK(settings::GetFloatingDisplayOpacity() == 50);
    for (DWORD value : {0u, 9u, 101u, 0xffffffffu})
    {
        test::WriteDword(L"FloatingDisplayOpacity", value);
        CHECK(settings::GetFloatingDisplayOpacity() == 100);
    }
    const wchar_t wrongType[] = L"40";
    test::WriteValue(productKey, L"FloatingDisplayOpacity", REG_SZ, wrongType, sizeof(wrongType));
    CHECK(settings::GetFloatingDisplayOpacity() == 100);
}

TEST_CASE("Theme follows the isolated Windows preference", "[integration][registry]")
{
    test::IsolatedRegistry registry;
    CHECK_FALSE(settings::IsLightTheme());
    for (DWORD value : {1u, 0u})
    {
        test::WriteValue(L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize", L"AppsUseLightTheme",
                         REG_DWORD, &value, sizeof(value));
        CHECK(settings::IsLightTheme() == (value != 0));
    }
}

TEST_CASE("GPU-only preferences survive an unavailable driver", "[integration][registry]")
{
    test::IsolatedRegistry registry;
    CHECK_FALSE(settings::LoadStartupToggles(true).gpuVisible);
    REQUIRE(settings::SetTaskbarGpuVisible(true));
    REQUIRE(settings::SetTaskbarRatesVisible(false));
    REQUIRE(settings::SetTaskbarMemoryVisible(false));
    const auto fallback = settings::LoadStartupToggles(false);
    CHECK(fallback.taskbarRatesVisible);
    CHECK_FALSE(fallback.gpuVisible);
    const auto restored = settings::LoadStartupToggles(true);
    CHECK_FALSE(restored.taskbarRatesVisible);
    CHECK(restored.gpuVisible);
}
