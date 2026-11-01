#pragma once

#include <windows.h>
#include <objbase.h>
#include <aclapi.h>
#include <catch2/catch_test_macros.hpp>
#include <cstdlib>
#include <string>

namespace test
{
// RegOverridePredefKey is process-wide; registry tests must not run on worker threads.
class IsolatedRegistry final
{
public:
    IsolatedRegistry()
    {
        GUID id{};
        REQUIRE(SUCCEEDED(CoCreateGuid(&id)));
        wchar_t identifier[39]{};
        REQUIRE(StringFromGUID2(id, identifier, 39) > 0);
        name_ = L"Software\\WinMonTests-" + std::wstring(identifier);
        REQUIRE(RegCreateKeyExW(HKEY_CURRENT_USER, name_.c_str(), 0, nullptr, 0, KEY_ALL_ACCESS, nullptr, &key_,
                                nullptr) == ERROR_SUCCESS);
        const auto status = RegOverridePredefKey(HKEY_CURRENT_USER, key_);
        if (status != ERROR_SUCCESS)
        {
            RegCloseKey(key_);
            RegDeleteTreeW(HKEY_CURRENT_USER, name_.c_str());
            FAIL("Cannot isolate HKCU: " << status);
        }
    }
    ~IsolatedRegistry()
    {
        if (RegOverridePredefKey(HKEY_CURRENT_USER, nullptr) != ERROR_SUCCESS)
            std::abort();
        RegCloseKey(key_);
        RegDeleteTreeW(HKEY_CURRENT_USER, name_.c_str());
    }
    IsolatedRegistry(const IsolatedRegistry&) = delete;
    IsolatedRegistry& operator=(const IsolatedRegistry&) = delete;

private:
    std::wstring name_;
    HKEY key_ = nullptr;
};

// Deny real registry writes while retaining a handle that can restore the old DACL.
class RegistryWriteBlock final
{
public:
    explicit RegistryWriteBlock(const wchar_t* path)
    {
        REQUIRE(RegOpenKeyExW(HKEY_CURRENT_USER, path, 0, KEY_ALL_ACCESS, &key_) == ERROR_SUCCESS);
        const auto status = GetSecurityInfo(key_, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr,
                                            &originalAcl_, nullptr, &originalSecurity_);
        if (status != ERROR_SUCCESS)
        {
            RegCloseKey(key_);
            FAIL("Read registry DACL: " << status);
        }
        SID_IDENTIFIER_AUTHORITY authority = SECURITY_WORLD_SID_AUTHORITY;
        PSID everyone = nullptr;
        if (!AllocateAndInitializeSid(&authority, 1, SECURITY_WORLD_RID, 0, 0, 0, 0, 0, 0, 0, &everyone))
        {
            LocalFree(originalSecurity_);
            RegCloseKey(key_);
            FAIL("Allocate Everyone SID");
        }
        EXPLICIT_ACCESSW access{};
        access.grfAccessPermissions = KEY_SET_VALUE | KEY_CREATE_SUB_KEY;
        access.grfAccessMode = DENY_ACCESS;
        access.Trustee.TrusteeForm = TRUSTEE_IS_SID;
        access.Trustee.ptstrName = static_cast<LPWSTR>(everyone);
        PACL denied = nullptr;
        auto result = SetEntriesInAclW(1, &access, originalAcl_, &denied);
        FreeSid(everyone);
        if (result == ERROR_SUCCESS)
            result =
                SetSecurityInfo(key_, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr, denied, nullptr);
        if (denied)
            LocalFree(denied);
        if (result != ERROR_SUCCESS)
        {
            LocalFree(originalSecurity_);
            RegCloseKey(key_);
            FAIL("Deny registry writes: " << result);
        }
    }
    ~RegistryWriteBlock()
    {
        if (SetSecurityInfo(key_, SE_REGISTRY_KEY, DACL_SECURITY_INFORMATION, nullptr, nullptr, originalAcl_,
                            nullptr) != ERROR_SUCCESS)
            std::abort();
        LocalFree(originalSecurity_);
        RegCloseKey(key_);
    }
    RegistryWriteBlock(const RegistryWriteBlock&) = delete;
    RegistryWriteBlock& operator=(const RegistryWriteBlock&) = delete;

private:
    HKEY key_ = nullptr;
    PACL originalAcl_ = nullptr;
    PSECURITY_DESCRIPTOR originalSecurity_ = nullptr;
};

inline void WriteValue(const wchar_t* key, const wchar_t* name, DWORD type, const void* data, DWORD size)
{
    REQUIRE(RegSetKeyValueW(HKEY_CURRENT_USER, key, name, type, data, size) == ERROR_SUCCESS);
}
inline void WriteDword(const wchar_t* name, DWORD value)
{
    WriteValue(L"Software\\Win Mon", name, REG_DWORD, &value, sizeof(value));
}
} // namespace test
