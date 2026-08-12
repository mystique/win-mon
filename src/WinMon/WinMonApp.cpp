#include "WinMonApp.h"

namespace
{
constexpr wchar_t kInstanceGuardName[] =
    L"Local\\WinMon.SingleInstance.{12CA7773-CC4F-4EED-9F2E-E534245324F9}";
constexpr wchar_t kProductName[] = L"Win Mon";
}

WinMonApp theApp;

BOOL WinMonApp::InitInstance()
{
    if (!CWinApp::InitInstance())
    {
        return FALSE;
    }

    instanceGuard_ = CreateMutexW(nullptr, FALSE, kInstanceGuardName);
    if (instanceGuard_ == nullptr)
    {
        MessageBoxW(
            nullptr,
            L"Win Mon could not create its Single Instance guard and will exit.",
            kProductName,
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        return FALSE;
    }

    if (GetLastError() == ERROR_ALREADY_EXISTS)
    {
        ReleaseInstanceGuard();
        return FALSE;
    }

    if (!trayWindow_.Initialize())
    {
        MessageBoxW(
            nullptr,
            L"Win Mon could not create its Tray Icon and will exit.",
            kProductName,
            MB_OK | MB_ICONERROR | MB_SETFOREGROUND);
        trayWindow_.Shutdown();
        ReleaseInstanceGuard();
        return FALSE;
    }

    m_pMainWnd = &trayWindow_;
    return TRUE;
}

int WinMonApp::ExitInstance()
{
    trayWindow_.Shutdown();
    ReleaseInstanceGuard();
    return CWinApp::ExitInstance();
}

void WinMonApp::ReleaseInstanceGuard() noexcept
{
    if (instanceGuard_ == nullptr)
    {
        return;
    }

    CloseHandle(instanceGuard_);
    instanceGuard_ = nullptr;
}
