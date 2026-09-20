#pragma once

#include <afxwin.h>
#include <string>
#include <vector>

#include "RateStrip.h"
#include "FloatingRateDisplay.h"
#include "NetworkObservationReader.h"
#include "../WinMonCore/WinMonCore.h"

class TrayMessageWindow final : public CWnd, private winmon::ShellSurface
{
public:
    TrayMessageWindow() noexcept;
    [[nodiscard]] bool Initialize();
    void Shutdown() noexcept;

private:
    [[nodiscard]] bool EnsureTrayIcon() override;
    void ForgetTrayIcon() noexcept override;
    [[nodiscard]] winmon::RateStripState RecreateRateStrip() override;
    [[nodiscard]] winmon::RateStripState RefreshRateStrip() override;
    void DestroyRateStrip() noexcept override;
    void RemoveTrayIcon() noexcept override;
    void ScheduleRecovery() noexcept override;
    void CancelRecovery() noexcept override;
    [[nodiscard]] HICON LoadTrayIcon() const noexcept;
    void UpdateTrayIconTheme() noexcept;
    void LoadSavedRateFont();
    void SaveFloatingRateDisplayPosition();

    [[nodiscard]] winmon::OperatorAction ShowOperatorMenu();
    void HandleOperatorMenuAction(winmon::OperatorAction action);
    void RequestExit();
    void ChooseRateFont();
    void SampleRates();
    afx_msg LRESULT OnTrayNotification(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnRightClickSpeedText(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnFloatingRateDisplayMoved(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnTaskbarCreated(WPARAM wParam, LPARAM lParam);
    afx_msg LRESULT OnDpiChanged(WPARAM wParam, LPARAM lParam);
    afx_msg void OnDisplayChange(UINT bitsPerPixel, int horizontalPixels, int verticalPixels);
    afx_msg void OnSettingChange(UINT flags, LPCTSTR section);
    afx_msg LRESULT OnThemeChanged();
    afx_msg void OnTimer(UINT_PTR timerId);

    DECLARE_MESSAGE_MAP()

    RateStrip rateStrip_;
    FloatingRateDisplay floatingRateDisplay_;
    winmon::ShellLifecycle shellLifecycle_;
    winmon::WinMonCore core_;
    NetworkObservationReader networkReader_;
    std::uint64_t displayedNetworkGeneration_ = 0;
    bool trayIconAdded_ = false;
    bool shellRecoveryTimerActive_ = false;
    bool shuttingDown_ = false;
    bool rateFontRecoveryPending_ = false;
};
