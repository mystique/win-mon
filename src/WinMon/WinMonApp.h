#pragma once

#include <afxwin.h>

#include "TrayMessageWindow.h"

class WinMonApp final : public CWinApp
{
public:
    BOOL InitInstance() override;
    int ExitInstance() override;

private:
    void ReleaseInstanceGuard() noexcept;

    HANDLE instanceGuard_ = nullptr;
    TrayMessageWindow trayWindow_;
};
