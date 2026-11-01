#include "platform/LaunchAtLogin.h"
#include "support/IsolatedRegistry.h"

namespace
{
constexpr auto runKey = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
}

TEST_CASE("Sign-in registration quotes the executable and preserves other entries", "[integration][registry][sign-in]")
{
    test::IsolatedRegistry registry;
    const wchar_t other[] = L"unrelated.exe";
    test::WriteValue(runKey, L"Other App", REG_SZ, other, sizeof(other));
    CHECK_FALSE(autostart::IsEnabled());
    REQUIRE(autostart::Enable());
    CHECK(autostart::IsEnabled());
    wchar_t actual[MAX_PATH + 3]{}, executable[MAX_PATH]{};
    DWORD size = sizeof(actual), type = 0;
    REQUIRE(RegGetValueW(HKEY_CURRENT_USER, runKey, L"Win Mon", RRF_RT_REG_SZ, &type, actual, &size) == ERROR_SUCCESS);
    REQUIRE(GetModuleFileNameW(nullptr, executable, MAX_PATH) > 0);
    CHECK(std::wstring(actual) == L"\"" + std::wstring(executable) + L"\"");
    CHECK(type == REG_SZ);
    REQUIRE(autostart::Disable());
    CHECK_FALSE(autostart::IsEnabled());
    CHECK(autostart::Disable());
    size = sizeof(actual);
    REQUIRE(RegGetValueW(HKEY_CURRENT_USER, runKey, L"Other App", RRF_RT_REG_SZ, nullptr, actual, &size) ==
            ERROR_SUCCESS);
    CHECK(std::wstring(actual) == other);
}

TEST_CASE("Sign-in registration reports denied writes without changing the entry", "[integration][registry][sign-in]")
{
    test::IsolatedRegistry registry;
    const wchar_t other[] = L"unrelated.exe";
    test::WriteValue(runKey, L"Other App", REG_SZ, other, sizeof(other));
    SECTION("Enabling fails without creating a value")
    {
        test::RegistryWriteBlock deny(runKey);
        CHECK_FALSE(autostart::Enable());
        CHECK_FALSE(autostart::IsEnabled());
    }
    SECTION("Disabling fails without removing the saved value")
    {
        REQUIRE(autostart::Enable());
        test::RegistryWriteBlock deny(runKey);
        CHECK_FALSE(autostart::Disable());
        CHECK(autostart::IsEnabled());
    }
}
