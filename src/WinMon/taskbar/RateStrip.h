// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include <afxwin.h>
#include <UIAutomation.h>
#include <atlbase.h>
#include <string>
#include <d2d1.h>
#include <wrl/client.h>

class RateStrip final : public CWnd
{
public:
    RateStrip() = default;
    RateStrip(const RateStrip&) = delete;
    RateStrip& operator=(const RateStrip&) = delete;
    ~RateStrip() override;
    [[nodiscard]] bool Embed(bool shouldShow);
    [[nodiscard]] bool Refresh();
    void Shutdown() noexcept;
    [[nodiscard]] static bool IsPrimaryBottomTaskbarAvailable() noexcept;
    [[nodiscard]] static SIZE SizeForDpi(UINT dpi) noexcept { return {MulDiv(94, dpi, 96), MulDiv(36, dpi, 96)}; }
    [[nodiscard]] bool RenderToDc(HDC dc, SIZE size, bool lightTheme, bool hitTestableBackground = false) noexcept;
    void SetRates(const std::wstring& uploadText, const std::wstring& downloadText) noexcept;
    [[nodiscard]] bool SetTaskbarDisplayOnLeft(bool left) noexcept;
    [[nodiscard]] bool IsTaskbarDisplayOnLeft() const noexcept { return taskbarDisplayOnLeft_; }
    [[nodiscard]] bool IsTaskbarSpaceFree(HWND taskbar, RECT screenCandidate) noexcept;
    [[nodiscard]] bool IsDisplayVisible() const noexcept { return visible_; }
    void SetContextMenuOwner(HWND owner, UINT notificationMessage) noexcept;
    void SetContextMenuEnabled(bool enabled) noexcept;
    [[nodiscard]] bool IsContextMenuEnabled() const noexcept { return contextMenuEnabled_; }

private:
    void ReleaseSurface() noexcept;
    [[nodiscard]] static HWND FindPrimaryBottomTaskbar() noexcept;
    [[nodiscard]] static HWND FindNotificationArea(HWND taskbar) noexcept;
    [[nodiscard]] CSize MeasureSize(HWND taskbar);
    [[nodiscard]] bool PlaceOnTaskbar(HWND taskbar, HWND notificationArea) noexcept;
    [[nodiscard]] bool Render() noexcept;
    [[nodiscard]] bool Relayout(HWND taskbar, HWND notificationArea) noexcept;
    afx_msg void OnPaint();
    afx_msg BOOL OnEraseBkgnd(CDC* deviceContext);
    afx_msg int OnMouseActivate(CWnd* desktopWindow, UINT hitTest, UINT message);
    afx_msg LRESULT OnNcHitTest(CPoint point);
    afx_msg void OnRButtonUp(UINT flags, CPoint point);
    DECLARE_MESSAGE_MAP()

    CComPtr<IUIAutomation2> automation_;
    bool apartmentOwned_ = false;
    Microsoft::WRL::ComPtr<ID2D1Factory> d2dFactory_;
    Microsoft::WRL::ComPtr<ID2D1DCRenderTarget> d2dTarget_;
    Microsoft::WRL::ComPtr<ID2D1SolidColorBrush> d2dBrush_;
    HDC surfaceDc_ = nullptr;
    HBITMAP surfaceBitmap_ = nullptr;
    HGDIOBJ surfacePrevious_ = nullptr;
    SIZE surfaceSize_{};
    CSize naturalSize_{};
    CSize size_{};
    std::wstring uploadText_ = L"0.0 K/s";
    std::wstring downloadText_ = L"0.0 K/s";
    HWND taskbar_ = nullptr; // Borrowed from Explorer; invalidated by shell recovery.
    HWND contextMenuOwner_ = nullptr;
    UINT contextMenuMessage_ = 0;
    bool contextMenuEnabled_ = false;
    bool taskbarDisplayOnLeft_ = false;
    bool visible_ = false;
};
