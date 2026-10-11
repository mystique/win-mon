#include "Settings.h"
#include <iostream>
#include <stdexcept>
#include <string>

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

// Redirect HKCU only inside this process so real operator settings cannot be changed.
class IsolatedRegistry final
{
public:
    IsolatedRegistry()
    {
        name_ = L"Software\\WinMonSettingsTests-" + std::to_wstring(GetCurrentProcessId());
        Require(RegCreateKeyExW(HKEY_CURRENT_USER, name_.c_str(), 0, nullptr, 0,
            KEY_ALL_ACCESS, nullptr, &key_, nullptr) == ERROR_SUCCESS, "isolated registry key");
        if (RegOverridePredefKey(HKEY_CURRENT_USER, key_) != ERROR_SUCCESS)
        {
            RegCloseKey(key_);
            RegDeleteTreeW(HKEY_CURRENT_USER, name_.c_str());
            throw std::runtime_error("HKCU redirection");
        }
    }
    ~IsolatedRegistry()
    {
        RegOverridePredefKey(HKEY_CURRENT_USER, nullptr);
        RegCloseKey(key_);
        RegDeleteTreeW(HKEY_CURRENT_USER, name_.c_str());
    }
private:
    std::wstring name_;
    HKEY key_ = nullptr;
};

int main()
{
    try
    {
        IsolatedRegistry registry;
        Require(settings::GetFloatingRateOpacity() == 100, "opacity defaults fully opaque");
        Require(settings::SetFloatingRateOpacity(45) && settings::GetFloatingRateOpacity() == 45, "opacity persisted");
        Require(!settings::SetFloatingRateOpacity(0) && !settings::SetFloatingRateOpacity(101), "opacity write range validated");
        for (DWORD invalidOpacity : {0u,9u,101u,0xffffffffu})
        {
            Require(RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\Win Mon",L"FloatingRateOpacity",REG_DWORD,
                &invalidOpacity,sizeof(invalidOpacity)) == ERROR_SUCCESS, "invalid opacity fixture");
            Require(settings::GetFloatingRateOpacity() == 100, "invalid opacity defaults opaque");
        }
        Require(settings::IsFloatingRateChartVisible(), "chart defaults on");
        Require(settings::SetFloatingRateChartVisible(false) && !settings::IsFloatingRateChartVisible(), "chart off persists");
        Require(settings::SetFloatingRateChartVisible(true) && settings::IsFloatingRateChartVisible(), "chart on persists");
        Require(settings::IsFloatingRateTextVisible(), "floating text defaults on");
        Require(settings::SetFloatingRateTextVisible(false) && !settings::IsFloatingRateTextVisible(), "hidden text persists");
        Require(settings::SetFloatingRateTextVisible(true) && settings::IsFloatingRateTextVisible(), "visible text persists");
        for (DWORD invalid : {2u, 0xffffffffu})
        {
            Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"FloatingRateTextVisible",
                REG_DWORD, &invalid, sizeof(invalid)) == ERROR_SUCCESS, "unknown text flag fixture");
            Require(settings::IsFloatingRateTextVisible(), "unknown text flag defaults visible");
        }
        Require(!settings::IsSpeedTextOnLeft(), "absent position defaults Right");
        Require(settings::SetSpeedTextOnLeft(true) && settings::IsSpeedTextOnLeft(), "Left survives reread");
        Require(settings::SetSpeedTextOnLeft(false) && !settings::IsSpeedTextOnLeft(), "Right survives reread");
        for (DWORD invalid : {2u, 0xffffffffu})
        {
            Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"SpeedTextPosition",
                REG_DWORD, &invalid, sizeof(invalid)) == ERROR_SUCCESS, "unknown value fixture");
            Require(!settings::IsSpeedTextOnLeft(), "unknown position defaults Right");
        }
        const wchar_t text[] = L"1";
        Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"SpeedTextPosition",
            REG_SZ, text, sizeof(text)) == ERROR_SUCCESS, "wrong type fixture");
        Require(!settings::IsSpeedTextOnLeft(), "wrong type defaults Right");
        Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"FloatingRateTextVisible",
            REG_SZ, text, sizeof(text)) == ERROR_SUCCESS, "wrong type text fixture");
        Require(settings::IsFloatingRateTextVisible(), "wrong type text defaults visible");
        const BYTE shortValue = 1;
        Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"SpeedTextPosition",
            REG_DWORD, &shortValue, sizeof(shortValue)) == ERROR_SUCCESS, "short DWORD fixture");
        Require(!settings::IsSpeedTextOnLeft(), "short DWORD defaults Right");
        std::cout << "SettingsTests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
