#pragma once
#include <afxwin.h>
#include "FloatingRateRenderer.h"
#include "RateFont.h"
#include <string>

class FloatingRateDisplay final : public CWnd
{
public:
    [[nodiscard]] bool ShowFloating(POINT position, bool restorePosition);
    [[nodiscard]] bool Refresh() noexcept { return Render(); }
    void Shutdown() noexcept;
    void ResetHistory() noexcept { floatingRenderer_.ResetHistory(); }
    void SetRates(const std::wstring& uploadText, const std::wstring& downloadText,
        double uploadBytesPerSecond, double downloadBytesPerSecond) noexcept;
    void SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept;
    void SetPositionChangedOwner(HWND owner, UINT notificationMessage) noexcept;
    [[nodiscard]] POINT GetFloatingPosition() const noexcept;
    [[nodiscard]] bool GetRateFont(RateFontSelection& selection) const noexcept { return rateFont_.Get(nullptr, selection); }
    [[nodiscard]] bool SetRateFont(const RateFontSelection& selection) noexcept { return UpdateRateFont(selection); }
    [[nodiscard]] bool ResetRateFont() noexcept { return UpdateRateFont(std::nullopt); }

private:
    bool UpdateRateFont(std::optional<RateFontSelection> selection) noexcept;
    bool RelayoutFloating() noexcept;
    bool Render() noexcept;
    void UpdatePulseTimer() noexcept;
    afx_msg void OnTimer(UINT_PTR timer);
    afx_msg void OnSettingChange(UINT flags, LPCTSTR section);
    afx_msg void OnMouseMove(UINT flags, CPoint point);
    afx_msg void OnMouseLeave();
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

    RateFont rateFont_;
    CSize size_{};
    std::wstring uploadText_ = L"0.0 K/s";
    std::wstring downloadText_ = L"0.0 K/s";
    CWnd floatingShadow_;
    FloatingRateRenderer floatingRenderer_;
    HWND contextMenuOwner_ = nullptr;
    UINT contextMenuMessage_ = 0;
    HWND positionChangedOwner_ = nullptr;
    UINT positionChangedMessage_ = 0;
    bool pulseTimerActive_ = false;
    bool hovering_ = false;
    float hoverProgress_ = 0;
    double pulsePhase_ = 0;
    ULONGLONG pulseTick_ = 0;
};
