#include "Autostart.h"

#include <windows.h>

namespace
{
constexpr wchar_t kRunKey[] = L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr wchar_t kValueName[] = L"Win Mon";

// RAII wrapper: opens HKCU\Run and closes on destruction.
// access must be KEY_QUERY_VALUE or KEY_SET_VALUE.
struct RunKey final
{
    explicit RunKey(REGSAM access) noexcept
    {
        status = RegOpenKeyExW(HKEY_CURRENT_USER, kRunKey, 0, access, &handle);
    }
    ~RunKey() noexcept
    {
        if (handle != nullptr) RegCloseKey(handle);
    }
    [[nodiscard]] bool ok() const noexcept { return status == ERROR_SUCCESS; }

    HKEY handle = nullptr;
    LSTATUS status = ERROR_SUCCESS;
};
}

namespace autostart
{

bool IsEnabled() noexcept
{
    const RunKey key{KEY_QUERY_VALUE};
    if (!key.ok()) return false;
    const LSTATUS queryStatus = RegQueryValueExW(key.handle, kValueName, nullptr, nullptr, nullptr, nullptr);
    return queryStatus == ERROR_SUCCESS;
}

bool Enable() noexcept
{
    wchar_t rawPath[MAX_PATH] = {};
    const DWORD charsWritten = GetModuleFileNameW(nullptr, rawPath, MAX_PATH);
    // == 0: API failure; == MAX_PATH: buffer full, path truncated — both invalid
    if (charsWritten == 0 || charsWritten == MAX_PATH) return false;

    // Build quoted path: "rawPath" (MAX_PATH + 2 quotes + NUL)
    wchar_t quotedPath[MAX_PATH + 3] = {};
    quotedPath[0] = L'"';
    if (wcscpy_s(quotedPath + 1, MAX_PATH + 1, rawPath) != 0) return false;
    quotedPath[charsWritten + 1] = L'"';
    // quotedPath[charsWritten + 2] is already zero from value-initialization

    const RunKey key{KEY_SET_VALUE};
    if (!key.ok()) return false;

    const DWORD byteCount = static_cast<DWORD>((wcslen(quotedPath) + 1) * sizeof(wchar_t));
    const LSTATUS setStatus = RegSetValueExW(
        key.handle, kValueName, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(quotedPath), byteCount);
    return setStatus == ERROR_SUCCESS;
}

bool Disable() noexcept
{
    const RunKey key{KEY_SET_VALUE};
    if (!key.ok()) return false;

    const LSTATUS deleteStatus = RegDeleteValueW(key.handle, kValueName);
    // ERROR_FILE_NOT_FOUND: already absent — treat as success
    return deleteStatus == ERROR_SUCCESS || deleteStatus == ERROR_FILE_NOT_FOUND;
}

}
