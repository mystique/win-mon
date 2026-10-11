// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once

#include <afxwin.h>

#include "app/TrayMessageWindow.h"

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
