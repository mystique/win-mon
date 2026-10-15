#include "settings/OperatorSettings.h"
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
        Require(settings::IsTaskbarTextVisible() && settings::IsTrayIconVisible(), "new entry toggles default visible");
        Require(settings::SetTaskbarTextVisible(false) && !settings::IsTaskbarTextVisible(), "taskbar hidden preference persists");
        Require(settings::SetTaskbarTextVisible(true) && settings::IsTaskbarTextVisible(), "taskbar shown preference persists");
        Require(settings::SetTrayIconVisible(false) && !settings::IsTrayIconVisible(), "tray hidden preference persists");
        Require(settings::SetTrayIconVisible(true) && settings::IsTrayIconVisible(), "tray shown preference persists");
        for (const auto* name : {L"TaskbarTextVisible", L"TrayIconVisible"})
        {
            const DWORD invalid = 2;
            Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", name, REG_DWORD,
                &invalid, sizeof(invalid)) == ERROR_SUCCESS, "invalid visibility fixture");
            Require(settings::IsTaskbarTextVisible() && settings::IsTrayIconVisible(), "invalid flags retain visible defaults");
            const wchar_t wrongType[] = L"0";
            Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", name, REG_SZ,
                wrongType, sizeof(wrongType)) == ERROR_SUCCESS, "wrong type visibility fixture");
            Require(settings::IsTaskbarTextVisible() && settings::IsTrayIconVisible(), "wrong type defaults visible");
        }
        Require(!settings::IsTaskbarContextMenuEnabled(), "taskbar context menu defaults off");
        Require(settings::SetTaskbarContextMenuEnabled(true) && settings::IsTaskbarContextMenuEnabled(), "taskbar context menu persists on");
        Require(settings::SetTaskbarContextMenuEnabled(false) && !settings::IsTaskbarContextMenuEnabled(), "taskbar context menu persists off");
        Require(!settings::IsFloatingDisplayEnabled(), "floating display defaults off");
        Require(settings::SetFloatingDisplayEnabled(true) && settings::IsFloatingDisplayEnabled(), "floating display persists on");
        Require(settings::SetFloatingDisplayEnabled(false) && !settings::IsFloatingDisplayEnabled(), "floating display persists off");
        for (unsigned flags = 0; flags < 16; ++flags)
        {
            Require(settings::SetTrayIconVisible((flags & 1) != 0) &&
                settings::SetFloatingDisplayEnabled((flags & 2) != 0) &&
                settings::SetTaskbarTextVisible((flags & 4) != 0) &&
                settings::SetTaskbarContextMenuEnabled((flags & 8) != 0), "menu access fixture");
            Require(settings::HasConfiguredMenuAccess() ==
                ((flags & 3) != 0 || (flags & 12) == 12), "configured menu access covers all entry combinations");
        }
        Require(settings::SetFloatingDisplayEnabled(false), "restore floating display fixture");
        POINT position{};
        Require(!settings::GetFloatingDisplayPosition(position), "floating position initially absent");
        Require(settings::SetFloatingDisplayPosition({120, 240}) &&
            settings::GetFloatingDisplayPosition(position) && position.x == 120 && position.y == 240,
            "floating display position persists");
        Require(settings::GetFloatingDisplayOpacity() == 100, "opacity defaults fully opaque");
        Require(settings::SetFloatingDisplayOpacity(40) && settings::GetFloatingDisplayOpacity() == 40, "opacity persisted");
        Require(!settings::SetFloatingDisplayOpacity(0) && !settings::SetFloatingDisplayOpacity(101) && !settings::SetFloatingDisplayOpacity(45), "opacity write range validated");
        DWORD legacyOpacity = 45;
        Require(RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\Win Mon",L"FloatingDisplayOpacity",REG_DWORD,
            &legacyOpacity,sizeof(legacyOpacity)) == ERROR_SUCCESS, "legacy slider fixture");
        Require(settings::GetFloatingDisplayOpacity() == 50, "legacy slider values round to nearest menu step");
        for (DWORD invalidOpacity : {0u,9u,101u,0xffffffffu})
        {
            Require(RegSetKeyValueW(HKEY_CURRENT_USER,L"Software\\Win Mon",L"FloatingDisplayOpacity",REG_DWORD,
                &invalidOpacity,sizeof(invalidOpacity)) == ERROR_SUCCESS, "invalid opacity fixture");
            Require(settings::GetFloatingDisplayOpacity() == 100, "invalid opacity defaults opaque");
        }
        Require(settings::AreFloatingChartsVisible(), "chart defaults on");
        Require(settings::SetFloatingChartsVisible(false) && !settings::AreFloatingChartsVisible(), "chart off persists");
        Require(settings::SetFloatingChartsVisible(true) && settings::AreFloatingChartsVisible(), "chart on persists");
        Require(settings::IsFloatingTextVisible(), "floating text defaults on");
        Require(settings::SetFloatingTextVisible(false) && !settings::IsFloatingTextVisible(), "hidden text persists");
        Require(settings::SetFloatingTextVisible(true) && settings::IsFloatingTextVisible(), "visible text persists");
        for (DWORD invalid : {2u, 0xffffffffu})
        {
            Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"FloatingTextVisible",
                REG_DWORD, &invalid, sizeof(invalid)) == ERROR_SUCCESS, "unknown text flag fixture");
            Require(settings::IsFloatingTextVisible(), "unknown text flag defaults visible");
        }
        Require(!settings::IsTaskbarDisplayOnLeft(), "absent position defaults Right");
        Require(settings::SetTaskbarDisplayOnLeft(true) && settings::IsTaskbarDisplayOnLeft(), "Left survives reread");
        Require(settings::SetTaskbarDisplayOnLeft(false) && !settings::IsTaskbarDisplayOnLeft(), "Right survives reread");
        for (DWORD invalid : {2u, 0xffffffffu})
        {
            Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"TaskbarDisplayPosition",
                REG_DWORD, &invalid, sizeof(invalid)) == ERROR_SUCCESS, "unknown value fixture");
            Require(!settings::IsTaskbarDisplayOnLeft(), "unknown position defaults Right");
        }
        const wchar_t text[] = L"1";
        Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"TaskbarDisplayPosition",
            REG_SZ, text, sizeof(text)) == ERROR_SUCCESS, "wrong type fixture");
        Require(!settings::IsTaskbarDisplayOnLeft(), "wrong type defaults Right");
        Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"FloatingTextVisible",
            REG_SZ, text, sizeof(text)) == ERROR_SUCCESS, "wrong type text fixture");
        Require(settings::IsFloatingTextVisible(), "wrong type text defaults visible");
        const BYTE shortValue = 1;
        Require(RegSetKeyValueW(HKEY_CURRENT_USER, L"Software\\Win Mon", L"TaskbarDisplayPosition",
            REG_DWORD, &shortValue, sizeof(shortValue)) == ERROR_SUCCESS, "short DWORD fixture");
        Require(!settings::IsTaskbarDisplayOnLeft(), "short DWORD defaults Right");
        std::cout << "SettingsTests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
