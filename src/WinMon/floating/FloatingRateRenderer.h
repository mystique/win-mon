// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include <Windows.h>
#include <d2d1.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <array>
#include <string>
#include <vector>

struct LedReadoutTransition
{
    std::wstring upload;
    std::wstring download;
    float progress = 1;
};
struct VisibilityTransition
{
    bool visible = true;
    float progress = 1;
    float start = 1;
    ULONGLONG tick = 0;
    float duration = 0;

    void Begin(bool show, ULONGLONG now) noexcept;
    void Advance(ULONGLONG now) noexcept;
    void Finish() noexcept;
};

struct VisibilityCycleStep
{
    bool changeText = false;
    bool visible = false;
    bool previousDirection = false;
};

struct VisibilityStateSnapshot
{
    VisibilityTransition text;
    VisibilityTransition chart;
    bool expandingVisibility = false;
};


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
    void SetRates(const std::wstring& upload, const std::wstring& download,
        double uploadBytesPerSecond, double downloadBytesPerSecond, ULONGLONG now) noexcept;
    void BeginVisibility(bool text, bool visible, ULONGLONG now, bool windowVisible, bool animationsEnabled) noexcept;
    [[nodiscard]] VisibilityStateSnapshot CaptureVisibilityState() const noexcept
    {
        return {textTransition_, chartTransition_, expandingVisibility_};
    }
    void RestoreVisibilityState(const VisibilityStateSnapshot& snapshot) noexcept
    {
        textTransition_ = snapshot.text;
        chartTransition_ = snapshot.chart;
        expandingVisibility_ = snapshot.expandingVisibility;
    }
    [[nodiscard]] bool IsTextVisible() const noexcept { return textTransition_.visible; }
    [[nodiscard]] bool IsChartVisible() const noexcept { return chartTransition_.visible; }
    [[nodiscard]] bool HasActiveVisibilityTransition() const noexcept
    {
        return textTransition_.duration > 0 || chartTransition_.duration > 0;
    }
    [[nodiscard]] VisibilityCycleStep NextVisibilityCycleStep() const noexcept;
    void CommitVisibilityCycleStep(bool applied, bool visible, bool previousDirection = false) noexcept;
    void StartAnimationClock(ULONGLONG now) noexcept { pulseTick_ = now; }
    void StopAnimationClock() noexcept { pulsePhase_ = 0; }
    void FinishTransitions(bool resetWater = false) noexcept;
    [[nodiscard]] bool SyncAnimationState(bool windowVisible, bool animationsEnabled) noexcept;
    [[nodiscard]] bool StepAnimation(ULONGLONG now, bool windowVisible, bool animationsEnabled) noexcept;
    [[nodiscard]] bool RenderCurrent(UINT dpi, bool pulseActive) noexcept;
    [[nodiscard]] SIZE CurrentSizeForDpi(UINT dpi) const noexcept
    {
        return SizeForDpi(dpi, textTransition_.progress, chartTransition_.progress);
    }
    [[nodiscard]] bool HitTestCurrent(POINT client, UINT dpi) const noexcept
    {
        return HitTest(client, dpi, textTransition_.progress, chartTransition_.progress);
    }
    [[nodiscard]] float TargetWaterLevel() const noexcept;
    bool Render(const std::wstring& upload, const std::wstring& download,
        UINT dpi, double pulsePhase = 0.5, float waterLevel = -1, float textProgress = 1, const LedReadoutTransition* transition = nullptr, float chartProgress = 1, float scrollProgress = 1) noexcept;
    double Activity() const noexcept;
    bool HasScrollingTrend() const noexcept;
    bool Present(HWND window, HWND shadow = nullptr, BYTE opacity = 255) noexcept;
    void ReleaseTarget() noexcept;
    const std::vector<DWORD>& Pixels() const;
    SIZE PixelSize() const noexcept { return size_; }
    static SIZE SizeForDpi(UINT dpi, float textProgress = 1, float chartProgress = 1) noexcept;
    static bool HitTest(POINT client, UINT dpi, float textProgress = 1, float chartProgress = 1) noexcept;
    static bool IsPositionVisible(RECT body, RECT workArea) noexcept;

private:
    template<class T> using ComPtr = Microsoft::WRL::ComPtr<T>;
    std::array<ComPtr<ID2D1PathGeometry>, 8> paths_;
    std::array<ComPtr<ID2D1LinearGradientBrush>, 3> gradients_;
    HDC surfaceDc_ = nullptr;
    HBITMAP surfaceBitmap_ = nullptr;
    HGDIOBJ surfacePrevious_ = nullptr;
    void* surfaceBits_ = nullptr;
    HWND presentedShadow_ = nullptr;
    BYTE presentedOpacity_ = 255;
    bool comInitialized_ = false;
    ComPtr<ID2D1Factory> factory_;
    ComPtr<IWICImagingFactory> imagingFactory_;
    ComPtr<IWICBitmap> bitmap_;
    ComPtr<ID2D1RenderTarget> target_;
    ComPtr<ID2D1SolidColorBrush> brush_;
    ComPtr<ID2D1StrokeStyle> stroke_;
    UINT dpi_ = 0;
    SIZE size_{};
    mutable std::vector<DWORD> pixels_;
    mutable bool pixelsDirty_ = true;
    std::vector<DWORD> bodyPixels_;
    std::vector<DWORD> shadowPixels_;
    std::array<double, 17> uploadHistory_{};
    std::array<double, 17> downloadHistory_{};
    size_t count_ = 0;
    double upload_ = 0;
    double download_ = 0;
    std::wstring uploadText_ = L"0.0 K/s";
    std::wstring downloadText_ = L"0.0 K/s";
    LedReadoutTransition readoutTransition_{};
    ULONGLONG readoutTick_ = 0;
    ULONGLONG chartSampleTick_ = 0;
    float chartScrollProgress_ = 1;
    VisibilityTransition textTransition_{};
    VisibilityTransition chartTransition_{};
    bool expandingVisibility_ = false;
    float waterLevel_ = 0;
    double pulsePhase_ = 0;
    ULONGLONG pulseTick_ = 0;
};
