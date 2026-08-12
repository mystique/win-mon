#pragma once

#include <afxwin.h>

#include "RateStrip.h"
#include "../WinMonCore/WinMonCore.h"

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
    void SampleRates();

    afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);
    afx_msg void OnTimer(UINT_PTR timerId);

    DECLARE_MESSAGE_MAP()

    RateStrip rateStrip_;
    winmon::WinMonCore core_;
    bool trayIconAdded_ = false;
};
