#pragma once

#include <afxwin.h>

#include <string>

class RateStrip final : public CWnd
{
public:
    [[nodiscard]] bool Embed(bool shouldShow);
    [[nodiscard]] bool Refresh();
    void Shutdown() noexcept;
    [[nodiscard]] static bool IsPrimaryBottomTaskbarAvailable() noexcept;
    void SetRates(const std::wstring& uploadText, const std::wstring& downloadText) noexcept;
    // Routes a Rate Strip right-click to owner as notificationMessage.
    void SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept;
    // Enables or disables mouse response; disabled keeps the strip display-only.
    void SetContextMenuEnabled(bool enabled) noexcept;
    [[nodiscard]] bool IsContextMenuEnabled() const noexcept { return contextMenuEnabled_; }

private:
    [[nodiscard]] static HWND FindPrimaryBottomTaskbar() noexcept;
    [[nodiscard]] static HWND FindNotificationArea(HWND taskbar) noexcept;
    [[nodiscard]] bool CreateSystemUiFont(HWND taskbar) noexcept;
    [[nodiscard]] CSize MeasureSize(HWND taskbar) const;
    [[nodiscard]] bool PlaceBesideNotificationArea(HWND taskbar, HWND notificationArea) noexcept;
    [[nodiscard]] bool Render() noexcept;
    [[nodiscard]] COLORREF ChooseTextColor(HWND taskbar, HWND notificationArea) const noexcept;
    [[nodiscard]] bool Relayout(HWND taskbar, HWND notificationArea) noexcept;

    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* deviceContext);
    afx_msg int OnMouseActivate(CWnd* desktopWindow, UINT hitTest, UINT message);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnRButtonUp(UINT flags, CPoint point);

    DECLARE_MESSAGE_MAP()

    CFont font_;
    CSize size_{};
    std::wstring uploadText_ = L"0.0K/s";
    std::wstring downloadText_ = L"0.0K/s";
    HWND taskbar_ = nullptr;
    COLORREF textColor_ = RGB(255, 255, 255);
    HWND contextMenuOwner_ = nullptr;
    UINT contextMenuMessage_ = 0;
    bool contextMenuEnabled_ = false;
};
