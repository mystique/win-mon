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
        const BYTE shortValue = 1;
        Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"SpeedTextPosition",
            REG_DWORD, &shortValue, sizeof(shortValue)) == ERROR_SUCCESS, "short DWORD fixture");
        Require(!settings::IsSpeedTextOnLeft(), "short DWORD defaults Right");
        std::cout << "SettingsTests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
