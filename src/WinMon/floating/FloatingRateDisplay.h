// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include <afxwin.h>
#include "floating/FloatingRateRenderer.h"
#include <string>
#include <functional>

class FloatingRateDisplay final : public CWnd
{
public:
    [[nodiscard]] bool ShowFloating(POINT position, bool restorePosition);
    [[nodiscard]] bool Refresh() noexcept { return Render(); }
    void Shutdown() noexcept;
    [[nodiscard]] bool SetTextVisible(bool visible) noexcept;
    [[nodiscard]] bool IsTextVisible() const noexcept { return floatingRenderer_.IsTextVisible(); }
    [[nodiscard]] bool SetChartVisible(bool visible) noexcept;
    [[nodiscard]] bool IsChartVisible() const noexcept { return floatingRenderer_.IsChartVisible(); }
    void SetVisibilityChangeHandler(std::function<bool(bool, bool)> handler) { visibilityChangeHandler_ = std::move(handler); }
    [[nodiscard]] bool SetOpacity(int percent) noexcept;
    int GetOpacity() const noexcept { return opacity_; }
    void ResetHistory() noexcept { floatingRenderer_.ResetHistory(); UpdatePulseTimer(); }
    void SetRates(const std::wstring& uploadText, const std::wstring& downloadText,
        double uploadBytesPerSecond, double downloadBytesPerSecond) noexcept;
    void SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept;
    void SetPositionChangedOwner(HWND owner, UINT notificationMessage) noexcept;
    [[nodiscard]] POINT GetFloatingPosition() const noexcept;

private:
    bool SetVisible(bool text, bool visible) noexcept;
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
    afx_msg void OnLButtonDblClk(UINT flags, CPoint point);
    afx_msg void OnMoving(UINT side, LPRECT rect);
    afx_msg void OnExitSizeMove();
    afx_msg void OnWindowPosChanged(WINDOWPOS* position);
    afx_msg LRESULT OnDpiChanged(WPARAM, LPARAM);
    afx_msg void OnDisplayChange(UINT, int, int);
    DECLARE_MESSAGE_MAP()

    CSize size_{};
    CWnd floatingShadow_;
    FloatingRateRenderer floatingRenderer_;
    HWND contextMenuOwner_ = nullptr;
    UINT contextMenuMessage_ = 0;
    HWND positionChangedOwner_ = nullptr;
    UINT positionChangedMessage_ = 0;
    int opacity_ = 100;
    std::function<bool(bool, bool)> visibilityChangeHandler_;
    bool positionDirty_ = false;
    bool pulseTimerActive_ = false;
};
