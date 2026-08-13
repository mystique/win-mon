#pragma once

#include <afxwin.h>
#include <string>
#include <vector>

#include "RateStrip.h"
#include "../WinMonCore/WinMonCore.h"

class TrayMessageWindow final : public CWnd
{
public:
    [[nodiscard]] bool Initialize();
    void Shutdown() noexcept;

private:
    [[nodiscard]] bool AddTrayIcon();
    [[nodiscard]] HICON LoadTrayIcon() const noexcept;
    void UpdateTrayIconTheme() noexcept;
    void RemoveTrayIcon() noexcept;
    void AttemptShellRecovery();
    void RecoverRateStrip();
    [[nodiscard]] bool ShouldRetryRateStrip() const noexcept;
    void ScheduleShellRecoveryRetry() noexcept;
    void CancelShellRecoveryRetry() noexcept;

    [[nodiscard]] UINT ShowOperatorMenu();
    void RequestExit();
    void SampleRates();
    [[nodiscard]] std::vector<winmon::NicSnapshot> ReadNicSnapshots(bool classifyForMenu = false) const;
    afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTaskbarCreated(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDisplayChange(UINT bitsPerPixel, int horizontalPixels, int verticalPixels);
    afx_msg void OnSettingChange(UINT flags, LPCTSTR section);
    afx_msg LRESULT OnThemeChanged();
    afx_msg void OnTimer(UINT_PTR timerId);

    DECLARE_MESSAGE_MAP()

    RateStrip rateStrip_;
    winmon::WinMonCore core_;
    std::vector<winmon::NicSnapshot> snapshots_;
    std::vector<std::string> menuNicIds_;
    bool trayIconAdded_ = false;
    bool shellRecoveryTimerActive_ = false;
    bool shuttingDown_ = false;
    bool autostartWasEnabled_ = false;
};
