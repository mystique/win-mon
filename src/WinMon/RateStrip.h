#pragma once

#include <afxwin.h>
#include "FloatingRateRenderer.h"

#include <array>
#include <cstddef>
#include <string>

struct RateFontSelection final
{
    LOGFONTW logFont{};
    int pointSizeTenths = 90;
};

class RateStrip final : public CWnd
{
public:
    [[nodiscard]] bool Embed(bool shouldShow);
    [[nodiscard]] bool ShowFloating(POINT position, bool restorePosition);
    [[nodiscard]] bool Refresh();
    void Shutdown() noexcept;
    void ResetHistory() noexcept { floatingRenderer_.ResetHistory(); }
    [[nodiscard]] static bool IsPrimaryBottomTaskbarAvailable() noexcept;
    void SetRates(
        const std::wstring& uploadText,
        const std::wstring& downloadText,
        double uploadBytesPerSecond,
        double downloadBytesPerSecond) noexcept;
    // Routes a Rate Strip right-click to owner as notificationMessage.
    void SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept;
    void SetPositionChangedOwner(HWND owner, UINT notificationMessage) noexcept;
    // Enables or disables mouse response; disabled keeps the strip display-only.
    void SetContextMenuEnabled(bool enabled) noexcept;
    [[nodiscard]] bool IsContextMenuEnabled() const noexcept { return contextMenuEnabled_; }
    [[nodiscard]] std::wstring GetRateFontName() const;
    [[nodiscard]] bool GetRateFont(RateFontSelection& selection) const noexcept;
    [[nodiscard]] bool SetRateFont(const RateFontSelection& selection) noexcept;
    [[nodiscard]] bool ResetRateFont() noexcept;
    [[nodiscard]] POINT GetFloatingPosition() const noexcept;

private:
    [[nodiscard]] static HWND FindPrimaryBottomTaskbar() noexcept;
    [[nodiscard]] static HWND FindNotificationArea(HWND taskbar) noexcept;
    [[nodiscard]] static bool LoadDefaultRateFont(HWND taskbar, RateFontSelection& selection) noexcept;
    [[nodiscard]] bool CreateRateFont(HWND taskbar) noexcept;
    [[nodiscard]] CSize MeasureSize(HWND taskbar) const;
    [[nodiscard]] bool PlaceBesideNotificationArea(HWND taskbar, HWND notificationArea) noexcept;
    [[nodiscard]] bool RelayoutFloating() noexcept;
    [[nodiscard]] bool Render() noexcept;
    [[nodiscard]] bool Relayout(HWND taskbar, HWND notificationArea) noexcept;

    void UpdatePulseTimer() noexcept;
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnSettingChange(UINT flags, LPCTSTR section);
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* deviceContext);
    afx_msg int OnMouseActivate(CWnd* desktopWindow, UINT hitTest, UINT message);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnRButtonUp(UINT flags, CPoint point);
    afx_msg void OnLButtonDown(UINT flags, CPoint point);
    afx_msg void OnExitSizeMove();
    afx_msg void OnWindowPosChanged(WINDOWPOS* position);
    afx_msg LRESULT OnDpiChanged(WPARAM, LPARAM);
    afx_msg void OnDisplayChange(UINT, int, int);

    DECLARE_MESSAGE_MAP()

    CFont font_;
    RateFontSelection selectedFont_{};
    CSize naturalSize_{};
    CSize size_{};
    std::wstring uploadText_ = L"0.0 K/s";
    std::wstring downloadText_ = L"0.0 K/s";
    CWnd floatingShadow_;
    FloatingRateRenderer floatingRenderer_;
    HWND taskbar_ = nullptr;
    COLORREF textColor_ = RGB(255, 255, 255);
    HWND contextMenuOwner_ = nullptr;
    UINT contextMenuMessage_ = 0;
    HWND positionChangedOwner_ = nullptr;
    UINT positionChangedMessage_ = 0;
    bool contextMenuEnabled_ = false;
    bool hasSelectedFont_ = false;
    bool floating_ = false;
    bool pulseTimerActive_ = false;
    double pulsePhase_ = 0;
    ULONGLONG pulseTick_ = 0;
};
