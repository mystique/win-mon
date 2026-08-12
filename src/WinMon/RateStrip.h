#pragma once

#include <afxwin.h>

#include <string>

class RateStrip final : public CWnd
{
public:
    [[nodiscard]] bool Embed(bool shouldShow);
    void Shutdown() noexcept;
    [[nodiscard]] static bool IsPrimaryBottomTaskbarAvailable() noexcept;
    void SetRates(const std::wstring& uploadText, const std::wstring& downloadText) noexcept;

private:
    [[nodiscard]] static HWND FindPrimaryBottomTaskbar() noexcept;
    [[nodiscard]] static HWND FindNotificationArea(HWND taskbar) noexcept;
    [[nodiscard]] bool CreateSystemUiFont(HWND taskbar) noexcept;
    [[nodiscard]] CSize MeasureSize(HWND taskbar) const;
    [[nodiscard]] bool PlaceBesideNotificationArea(HWND taskbar, HWND notificationArea) noexcept;
    [[nodiscard]] bool Render() noexcept;
    [[nodiscard]] bool Relayout(HWND taskbar, HWND notificationArea) noexcept;

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* deviceContext);
    afx_msg int OnMouseActivate(CWnd* desktopWindow, UINT hitTest, UINT message);
    afx_msg LRESULT OnNcHitTest(CPoint point);

    DECLARE_MESSAGE_MAP()

    CFont font_;
    CSize size_{};
    std::wstring uploadText_ = L"0.0K/s";
    std::wstring downloadText_ = L"0.0K/s";
    HWND taskbar_ = nullptr;
};
