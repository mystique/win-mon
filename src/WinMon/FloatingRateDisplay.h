#pragma once
#include <afxwin.h>
#include "FloatingRateRenderer.h"
#include <string>

class FloatingRateDisplay final : public CWnd
{
public:
    [[nodiscard]] bool ShowFloating(POINT position, bool restorePosition);
    [[nodiscard]] bool Refresh() noexcept { return Render(); }
    void Shutdown() noexcept;
    [[nodiscard]] bool SetTextVisible(bool visible) noexcept;
    [[nodiscard]] bool IsTextVisible() const noexcept { return textVisible_; }
    void ResetHistory() noexcept { floatingRenderer_.ResetHistory(); waterLevel_ = 0; pulsePhase_ = 0; UpdatePulseTimer(); }
    void SetRates(const std::wstring& uploadText, const std::wstring& downloadText,
        double uploadBytesPerSecond, double downloadBytesPerSecond) noexcept;
    void SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept;
    void SetPositionChangedOwner(HWND owner, UINT notificationMessage) noexcept;
    [[nodiscard]] POINT GetFloatingPosition() const noexcept;

private:
    void AdvanceTextTransition(ULONGLONG now) noexcept;
    bool RelayoutFloating() noexcept;
    bool Render() noexcept;
    void UpdatePulseTimer() noexcept;
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnSettingChange(UINT flags, LPCTSTR section);
    afx_msg void OnShowWindow(BOOL show, UINT status);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* deviceContext);
    afx_msg int OnMouseActivate(CWnd* desktopWindow, UINT hitTest, UINT message);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnRButtonUp(UINT flags, CPoint point);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnMoving(UINT side, LPRECT rect);
    afx_msg void OnExitSizeMove();
    afx_msg void OnWindowPosChanged(WINDOWPOS* position);
    afx_msg LRESULT OnDpiChanged(WPARAM, LPARAM);
    afx_msg void OnDisplayChange(UINT, int, int);
    DECLARE_MESSAGE_MAP()

    CSize size_{};
    std::wstring uploadText_ = L"0.0 K/s";
    std::wstring downloadText_ = L"0.0 K/s";
    CWnd floatingShadow_;
    FloatingRateRenderer floatingRenderer_;
    HWND contextMenuOwner_ = nullptr;
    UINT contextMenuMessage_ = 0;
    HWND positionChangedOwner_ = nullptr;
    UINT positionChangedMessage_ = 0;
    LedReadoutTransition readoutTransition_;
    ULONGLONG readoutTick_ = 0;
    bool textVisible_ = true;
    float textProgress_ = 1;
    float textStart_ = 1;
    ULONGLONG textTick_ = 0;
    float textDuration_ = 0;
    bool positionDirty_ = false;
    bool pulseTimerActive_ = false;
    float waterLevel_ = 0;
    double pulsePhase_ = 0;
    ULONGLONG pulseTick_ = 0;
};
