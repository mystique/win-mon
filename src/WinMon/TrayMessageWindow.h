#pragma once

#include <afxwin.h>

#include "RateStrip.h"

class TrayMessageWindow final : public CWnd
{
public:
    [[nodiscard]] bool Initialize();
    void Shutdown() noexcept;

private:
    [[nodiscard]] bool AddTrayIcon();
    void RemoveTrayIcon() noexcept;
    [[nodiscard]] UINT ShowOperatorMenu();
    void RequestExit();

    afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);

    DECLARE_MESSAGE_MAP()

    RateStrip rateStrip_;
    bool trayIconAdded_ = false;
};
