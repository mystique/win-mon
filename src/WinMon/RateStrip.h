#pragma once
#include <afxwin.h>
#include "RateFont.h"
#include <string>

class RateStrip final : public CWnd
{
public:
    [[nodiscard]] bool Embed(bool shouldShow);
    [[nodiscard]] bool Refresh();
    void Shutdown() noexcept;
    [[nodiscard]] static bool IsPrimaryBottomTaskbarAvailable() noexcept;
    void SetRates(const std::wstring& uploadText, const std::wstring& downloadText) noexcept;
    void SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept;
    void SetContextMenuEnabled(bool enabled) noexcept;
    [[nodiscard]] bool IsContextMenuEnabled() const noexcept { return contextMenuEnabled_; }
    [[nodiscard]] std::wstring GetRateFontName() const;
    [[nodiscard]] bool GetRateFont(RateFontSelection& selection) const noexcept;
    [[nodiscard]] bool SetRateFont(const RateFontSelection& selection) noexcept { return UpdateRateFont(selection); }
    [[nodiscard]] bool ResetRateFont() noexcept { return UpdateRateFont(std::nullopt); }

private:
    bool UpdateRateFont(std::optional<RateFontSelection> selection) noexcept;
    [[nodiscard]] static HWND FindPrimaryBottomTaskbar() noexcept;
    [[nodiscard]] static HWND FindNotificationArea(HWND taskbar) noexcept;
    [[nodiscard]] bool CreateRateFont(HWND taskbar) noexcept;
    [[nodiscard]] CSize MeasureSize(HWND taskbar) const;
    [[nodiscard]] bool PlaceBesideNotificationArea(HWND taskbar, HWND notificationArea) noexcept;
    [[nodiscard]] bool Render() noexcept;
    [[nodiscard]] bool Relayout(HWND taskbar, HWND notificationArea) noexcept;
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* deviceContext);
    afx_msg int OnMouseActivate(CWnd* desktopWindow, UINT hitTest, UINT message);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnRButtonUp(UINT flags, CPoint point);
    DECLARE_MESSAGE_MAP()

    CFont font_;
    RateFont rateFont_;
    CSize naturalSize_{};
    CSize size_{};
    std::wstring uploadText_ = L"0.0 K/s";
    std::wstring downloadText_ = L"0.0 K/s";
    HWND taskbar_ = nullptr;
    COLORREF textColor_ = RGB(255, 255, 255);
    HWND contextMenuOwner_ = nullptr;
    UINT contextMenuMessage_ = 0;
    bool contextMenuEnabled_ = false;
};
