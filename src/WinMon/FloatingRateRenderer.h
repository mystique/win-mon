#pragma once
#include <Windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <string>
#include <vector>

// One concrete offscreen renderer, also used by the layered desktop window.
class FloatingRateRenderer final
{
public:
    FloatingRateRenderer() = default;
    FloatingRateRenderer(const FloatingRateRenderer&) = delete;
    FloatingRateRenderer& operator=(const FloatingRateRenderer&) = delete;
    ~FloatingRateRenderer();
    void AddSample(double upload, double download) noexcept;
    void ResetHistory() noexcept;
    bool Render(const std::wstring& upload, const std::wstring& download,
        const LOGFONTW& font, UINT dpi, double pulsePhase = 0.5, float hoverProgress = 1) noexcept;
    double Activity() const noexcept;
    bool Present(HWND window, HWND shadow = nullptr) noexcept;
    void ReleaseTarget() noexcept;
    const std::vector<DWORD>& Pixels() const;
    SIZE PixelSize() const noexcept { return size_; }
    static SIZE SizeForDpi(UINT dpi) noexcept;
    static bool HitTest(POINT client, UINT dpi) noexcept;
    static bool IsPositionVisible(RECT body, RECT workArea) noexcept;

private:
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    struct TextLayout
    {
        ComPtr<IDWriteTextLayout> layout;
        std::wstring text;
        float width = 0, height = 0, measuredWidth = 0;
    };
    std::array<TextLayout, 8> layouts_;
    std::array<ComPtr<ID2D1PathGeometry>, 6> paths_;
    HDC surfaceDc_ = nullptr;
    HBITMAP surfaceBitmap_ = nullptr;
    HGDIOBJ surfacePrevious_ = nullptr;
    void* surfaceBits_ = nullptr;
    HWND presentedShadow_ = nullptr;
    bool comInitialized_ = false;
    ComPtr<ID2D1Factory> factory_;
    ComPtr<IDWriteFactory> textFactory_;
    ComPtr<IWICImagingFactory> imagingFactory_;
    ComPtr<IWICBitmap> bitmap_;
    ComPtr<ID2D1RenderTarget> target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1StrokeStyle> stroke_;
    std::array<ComPtr<IDWriteTextFormat>, 3> formats_;
    LOGFONTW font_{};
    UINT dpi_ = 0;
    SIZE size_{};
    mutable std::vector<DWORD> pixels_;
    mutable bool pixelsDirty_ = true;
    std::vector<DWORD> bodyPixels_;
    std::vector<DWORD> shadowPixels_;
    std::array<double, 48> uploadHistory_{};
    std::array<double, 48> downloadHistory_{};
    size_t count_ = 0;
    double upload_ = 0;
    double download_ = 0;
};
