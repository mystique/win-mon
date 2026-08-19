#include "Settings.h"
#include "RateStrip.h"

#include <windows.h>

#include <cwchar>

namespace
{
constexpr wchar_t kProductKey[] = L"Software\\Win Mon";
constexpr wchar_t kRightClickSpeedTextValue[] = L"RightClickSpeedText";
constexpr wchar_t kFloatingRateDisplayValue[] = L"FloatingRateDisplay";
constexpr wchar_t kFloatingRateDisplayPositionValue[] = L"FloatingRateDisplayPosition";
constexpr wchar_t kRateFontValue[] = L"RateFont";
constexpr DWORD kRateFontVersion = 1;
constexpr int kMinimumPointSizeTenths = 60;
constexpr int kMaximumPointSizeTenths = 120;

struct PersistedRateFont final
{
    DWORD version = kRateFontVersion;
    LOGFONTW logFont{};
    DWORD pointSizeTenths = 90;
};

bool ReadFlag(const wchar_t* valueName, bool& enabled) noexcept
{
    DWORD value = 0;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        valueName,
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    if (status != ERROR_SUCCESS)
    {
        return false;
    }
    enabled = value != 0;
    return true;
}

bool WriteFlag(const wchar_t* valueName, bool enabled) noexcept
{
    const DWORD value = enabled ? 1u : 0u;
    // RegSetKeyValueW creates the product key on first write.
    const LSTATUS status = RegSetKeyValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        valueName,
        REG_DWORD,
        &value,
        sizeof(value));
    return status == ERROR_SUCCESS;
}
}

namespace settings
{

bool IsRightClickSpeedTextEnabled() noexcept
{
    bool enabled = false;
    return ReadFlag(kRightClickSpeedTextValue, enabled) && enabled;
}

bool SetRightClickSpeedTextEnabled(bool enabled) noexcept
{
    return WriteFlag(kRightClickSpeedTextValue, enabled);
}

bool IsFloatingRateDisplayEnabled() noexcept
{
    bool enabled = false;
    return ReadFlag(kFloatingRateDisplayValue, enabled) && enabled;
}

bool SetFloatingRateDisplayEnabled(bool enabled) noexcept
{
    return WriteFlag(kFloatingRateDisplayValue, enabled);
}

bool GetFloatingRateDisplayPosition(POINT& position) noexcept
{
    DWORD size = sizeof(position);
    return RegGetValueW(
               HKEY_CURRENT_USER,
               kProductKey,
               kFloatingRateDisplayPositionValue,
               RRF_RT_REG_BINARY,
               nullptr,
               &position,
               &size) == ERROR_SUCCESS && size == sizeof(position);
}

bool SetFloatingRateDisplayPosition(POINT position) noexcept
{
    return RegSetKeyValueW(
               HKEY_CURRENT_USER,
               kProductKey,
               kFloatingRateDisplayPositionValue,
               REG_BINARY,
               &position,
               sizeof(position)) == ERROR_SUCCESS;
}

bool GetRateFont(RateFontSelection& selection) noexcept
{
    PersistedRateFont persisted;
    DWORD size = sizeof(persisted);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        kRateFontValue,
        RRF_RT_REG_BINARY,
        nullptr,
        &persisted,
        &size);
    if (status != ERROR_SUCCESS || size != sizeof(persisted) ||
        persisted.version != kRateFontVersion ||
        persisted.pointSizeTenths < kMinimumPointSizeTenths ||
        persisted.pointSizeTenths > kMaximumPointSizeTenths)
    {
        return false;
    }

    if (persisted.logFont.lfFaceName[0] == L'\0' ||
        wmemchr(persisted.logFont.lfFaceName, L'\0', LF_FACESIZE) == nullptr)
    {
        return false;
    }

    selection.logFont = persisted.logFont;
    selection.pointSizeTenths = static_cast<int>(persisted.pointSizeTenths);
    return true;
}

bool SetRateFont(const RateFontSelection& selection) noexcept
{
    if (selection.logFont.lfFaceName[0] == L'\0' ||
        selection.pointSizeTenths < kMinimumPointSizeTenths ||
        selection.pointSizeTenths > kMaximumPointSizeTenths)
    {
        return false;
    }

    PersistedRateFont persisted;
    persisted.logFont = selection.logFont;
    persisted.logFont.lfFaceName[LF_FACESIZE - 1] = L'\0';
    persisted.pointSizeTenths = static_cast<DWORD>(selection.pointSizeTenths);
    return RegSetKeyValueW(
        HKEY_CURRENT_USER,
        kProductKey,
        kRateFontValue,
        REG_BINARY,
        &persisted,
        sizeof(persisted)) == ERROR_SUCCESS;
}

}
