#include "FloatingRateRenderer.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kBodyScale = 12.0f / 11.0f; // Preserve the enlarged ring and text while narrowing the capsule.

void Check(HRESULT result) { if (FAILED(result)) throw result; }
double Rate(double value) noexcept { return std::isfinite(value) && value > 0 ? value : 0; }
}

FloatingRateRenderer::~FloatingRateRenderer()
{
    ReleaseTarget();
    for (auto& path : paths_) path.Reset();
    stroke_.Reset();
    imagingFactory_.Reset();
    factory_.Reset();
    if (comInitialized_) CoUninitialize();
}

SIZE FloatingRateRenderer::SizeForDpi(UINT dpi) noexcept
{
    return {MulDiv(138, static_cast<int>(dpi), 96), MulDiv(54, static_cast<int>(dpi), 96)};
}

bool FloatingRateRenderer::HitTest(POINT client, UINT dpi) noexcept
{
    if (!dpi) return false;
    const float x = client.x * 96.0f / dpi - 3;
    const float y = client.y * 96.0f / dpi - 27;
    const float dx = x - std::clamp(x, 24.0f, 108.0f);
    return dx * dx + y * y <= 24 * 24;
}

bool FloatingRateRenderer::IsPositionVisible(RECT body, RECT workArea) noexcept
{
    RECT visible{};
    return IntersectRect(&visible, &body, &workArea) &&
        visible.right - visible.left >= std::min<LONG>(48, body.right - body.left) &&
        visible.bottom - visible.top >= std::min<LONG>(48, body.bottom - body.top);
}

void FloatingRateRenderer::ResetHistory() noexcept
{
    for (size_t i = 2; i < paths_.size(); ++i) paths_[i].Reset();
    count_ = 0;
    upload_ = download_ = 0;
    uploadHistory_.fill(0);
    downloadHistory_.fill(0);
}

void FloatingRateRenderer::AddSample(double upload, double download) noexcept
{
    for (size_t i = 2; i < paths_.size(); ++i) paths_[i].Reset();
    upload_ = Rate(upload);
    download_ = Rate(download);
    const double up = count_ ? uploadHistory_.back() * 0.65 + upload_ * 0.35 : upload_;
    const double down = count_ ? downloadHistory_.back() * 0.65 + download_ * 0.35 : download_;
    std::rotate(uploadHistory_.begin(), uploadHistory_.begin() + 1, uploadHistory_.end());
    std::rotate(downloadHistory_.begin(), downloadHistory_.begin() + 1, downloadHistory_.end());
    uploadHistory_.back() = up;
    downloadHistory_.back() = down;
    count_ = std::min(count_ + 1, uploadHistory_.size());
}

double FloatingRateRenderer::Activity() const noexcept
{
    if (upload_ == 0 && download_ == 0) return 0;
    double peak = 1000;
    for (size_t i = 48 - count_; i < 48; ++i)
        peak = std::max({peak, uploadHistory_[i], downloadHistory_[i]});
    return std::clamp(std::max(uploadHistory_.back(), downloadHistory_.back()) / peak, 0.0, 1.0);
}

void FloatingRateRenderer::ReleaseTarget() noexcept
{
    if (surfacePrevious_) SelectObject(surfaceDc_, surfacePrevious_);
    if (surfaceBitmap_) DeleteObject(surfaceBitmap_);
    if (surfaceDc_) DeleteDC(surfaceDc_);
    surfaceDc_ = nullptr;
    surfaceBitmap_ = nullptr;
    surfacePrevious_ = nullptr;
    surfaceBits_ = nullptr;
    presentedShadow_ = nullptr;
    shadowPixels_.clear();
    bodyPixels_.clear();
    pixels_.clear();
    pixelsDirty_ = true;
    for (auto& gradient : gradients_) gradient.Reset();
    brush_.Reset();
    target_.Reset();
    bitmap_.Reset();
    dpi_ = 0;
}

bool FloatingRateRenderer::Render(const std::wstring&, const std::wstring&,
    const LOGFONTW&, UINT dpi, double pulsePhase, float waterLevel) noexcept
{
    try
    {
        if (dpi < 48 || dpi > 768) return false;
        if (!factory_)
        {
            if (!comInitialized_)
            {
                const HRESULT result = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
                if (result != RPC_E_CHANGED_MODE) Check(result);
                comInitialized_ = SUCCEEDED(result);
            }
            Check(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory_.ReleaseAndGetAddressOf()));
        }
        if (!imagingFactory_)
            Check(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                IID_PPV_ARGS(imagingFactory_.ReleaseAndGetAddressOf())));
        if (!stroke_)
        {
            auto properties = D2D1::StrokeStyleProperties();
            properties.startCap = properties.endCap = D2D1_CAP_STYLE_ROUND;
            properties.lineJoin = D2D1_LINE_JOIN_ROUND;
            Check(factory_->CreateStrokeStyle(properties, nullptr, 0, stroke_.ReleaseAndGetAddressOf()));
        }
        const SIZE size = SizeForDpi(dpi);
        if (!target_ || dpi_ != dpi)
        {
            ReleaseTarget();
            Check(imagingFactory_->CreateBitmap(size.cx, size.cy, GUID_WICPixelFormat32bppPBGRA,
                WICBitmapCacheOnLoad, &bitmap_));
            auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED),
                static_cast<float>(dpi), static_cast<float>(dpi));
            Check(factory_->CreateWicBitmapRenderTarget(bitmap_.Get(), properties, &target_));
            Check(target_->CreateSolidColorBrush(D2D1::ColorF(0xffffff), &brush_));
            target_->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
            const UINT32 colors[]{0x61e5b7, 0x32bdeb, 0x43cfc9};
            for (size_t i = 0; i < gradients_.size(); ++i)
            {
                const D2D1_GRADIENT_STOP stops[]{
                    {0, D2D1::ColorF(colors[i], i == 2 ? 0.95f : 0.55f)},
                    {1, D2D1::ColorF(i == 2 ? 0x1685b5 : colors[i], i == 2 ? 0.7f : 0.03f)}};
                ComPtr<ID2D1GradientStopCollection> collection;
                Check(target_->CreateGradientStopCollection(stops, 2, &collection));
                Check(target_->CreateLinearGradientBrush(
                    D2D1::LinearGradientBrushProperties(D2D1::Point2F(0, 9), D2D1::Point2F(0, 40)),
                    collection.Get(), &gradients_[i]));
            }
            dpi_ = dpi;
        }
        const auto color = [&](UINT32 rgb, float alpha = 1) { brush_->SetColor(D2D1::ColorF(rgb, alpha)); };
        const auto copyPixels = [&](std::vector<DWORD>& pixels)
        {
            pixels.resize(static_cast<size_t>(size.cx) * size.cy);
            Check(bitmap_->CopyPixels(nullptr, size.cx * 4, static_cast<UINT>(pixels.size() * 4),
                reinterpret_cast<BYTE*>(pixels.data())));
        };
        if (shadowPixels_.empty())
        {
            target_->SetTransform(D2D1::Matrix3x2F::Identity());
            target_->BeginDraw();
            target_->Clear(D2D1::ColorF(0, 0.0f));
            // The shadow only changes with DPI.
            color(0x000000, 0.06f);
            target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(1, 1, 137, 53), 26, 26), brush_.Get());
            color(0x000000, 0.10f);
            target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(2, 2, 136, 52), 25, 25), brush_.Get());
            Check(target_->EndDraw());
            copyPixels(shadowPixels_);
        }
        target_->BeginDraw();
        target_->Clear(D2D1::ColorF(0, 0.0f));
        // Scale vectors and glyphs before rasterization, leaving the shadow margin at 3 DIP.
        target_->SetTransform(D2D1::Matrix3x2F::Scale(kBodyScale, kBodyScale, D2D1::Point2F(3, 3)));
        color(0x222e34);
        target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(3, 3, 124, 47), 22, 22), brush_.Get());
        double maximum = 1000;
        for (size_t i = 48 - count_; i < 48; ++i)
            maximum = std::max({maximum, uploadHistory_[i], downloadHistory_[i]});
        const auto arc = [&](size_t slot, double ratio, bool up, UINT32 rgb)
        {
            if (ratio <= 0) return;
            // Leave about 2 DIP of clear space after accounting for the round caps.
            constexpr double gapAngle = 0.13;
            const double angle = gapAngle + std::clamp(ratio, 0.0, 1.0) * (3.141592653589793 - 2 * gapAngle);
            auto& path = paths_[slot];
            if (!path)
            {
                ComPtr<ID2D1GeometrySink> sink;
                Check(factory_->CreatePathGeometry(&path));
                Check(path->Open(&sink));
                sink->BeginFigure(D2D1::Point2F(25 + 17.25f * static_cast<float>(std::cos(gapAngle)),
                    25 + (up ? -17.25f : 17.25f) * static_cast<float>(std::sin(gapAngle))), D2D1_FIGURE_BEGIN_HOLLOW);
                sink->AddArc(D2D1::ArcSegment(
                    D2D1::Point2F(25 + 17.25f * static_cast<float>(std::cos(angle)),
                        25 + (up ? -17.25f : 17.25f) * static_cast<float>(std::sin(angle))),
                    D2D1::SizeF(17.25f, 17.25f), 0,
                    up ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE,
                    D2D1_ARC_SIZE_SMALL));
                sink->EndFigure(D2D1_FIGURE_END_OPEN);
                Check(sink->Close());
            }
            color(rgb);
            target_->DrawGeometry(path.Get(), brush_.Get(), 2.5f, stroke_.Get());
        };
        arc(0, 1, true, 0x46565f);
        arc(1, 1, false, 0x46565f);
        arc(2, upload_ > 0 ? uploadHistory_.back() / maximum : 0, true, 0x61e5b7);
        arc(3, download_ > 0 ? downloadHistory_.back() / maximum : 0, false, 0x32bdeb);
        const float level = std::isfinite(waterLevel) && waterLevel >= 0
            ? std::clamp(waterLevel, 0.0f, 1.0f) : static_cast<float>(Activity());
        if (level > 0)
        {
            const float phase = static_cast<float>(std::isfinite(pulsePhase) ? pulsePhase : 0.5);
            ComPtr<ID2D1EllipseGeometry> circle;
            Check(factory_->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(25, 25), 14.5f, 14.5f), &circle));
            ComPtr<ID2D1PathGeometry> water;
            ComPtr<ID2D1GeometrySink> sink;
            Check(factory_->CreatePathGeometry(&water));
            Check(water->Open(&sink));
            sink->BeginFigure(D2D1::Point2F(10, 40), D2D1_FIGURE_BEGIN_FILLED);
            const float amplitude = 1.2f * std::sin(level * 3.14159265f);
            for (int x = 10; x <= 40; ++x)
                sink->AddLine(D2D1::Point2F(static_cast<float>(x), 39.5f - 29 * level +
                    amplitude * std::sin((x - 10) * 0.25f + phase * 6.2831853f)));
            sink->AddLine(D2D1::Point2F(40, 40));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            Check(sink->Close());
            target_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), circle.Get()), nullptr);
            target_->FillGeometry(water.Get(), gradients_[2].Get());
            target_->PopLayer();
        }
        if (count_ > 1)
        {
            const auto trend = [&](size_t slot, const auto& history, UINT32 rgb)
            {

                auto& path = paths_[slot];
                if (!path)
                {
                    const auto point = [&](size_t i)
                    {
                        return D2D1::Point2F(47 + 66.0f * static_cast<float>(i) / 47,
                            40 - 31 * static_cast<float>(history[i] / maximum));
                    };
                    const size_t first = 48 - count_;
                    const auto tangent = [&](size_t i)
                    {
                        if (i == first) return point(i + 1).y - point(i).y;
                        if (i == 47) return point(i).y - point(i - 1).y;
                        const float before = point(i).y - point(i - 1).y;
                        const float after = point(i + 1).y - point(i).y;
                        // Monotone Hermite slopes retain every sampled peak and valley,
                        // without interpolation overshoot or additional rate smoothing.
                        return before * after > 0 ? 2 * before * after / (before + after) : 0.0f;
                    };
                    ComPtr<ID2D1GeometrySink> sink;
                    Check(factory_->CreatePathGeometry(&path));
                    Check(path->Open(&sink));
                    ComPtr<ID2D1GeometrySink> fillSink;
                    Check(factory_->CreatePathGeometry(&paths_[slot + 2]));
                    Check(paths_[slot + 2]->Open(&fillSink));
                    fillSink->BeginFigure(point(first), D2D1_FIGURE_BEGIN_FILLED);
                    sink->BeginFigure(point(first), D2D1_FIGURE_BEGIN_HOLLOW);
                    for (size_t i = first + 1; i < 48; ++i)
                    {
                        const auto from = point(i - 1), to = point(i);
                        const float third = (to.x - from.x) / 3;
                        const auto segment = D2D1::BezierSegment(
                            D2D1::Point2F(from.x + third, from.y + tangent(i - 1) / 3),
                            D2D1::Point2F(to.x - third, to.y - tangent(i) / 3), to);
                        sink->AddBezier(segment);
                        fillSink->AddBezier(segment);
                    }
                    fillSink->AddLine(D2D1::Point2F(113, 40));
                    fillSink->AddLine(D2D1::Point2F(point(first).x, 40));
                    fillSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    Check(fillSink->Close());
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                    Check(sink->Close());
                }
                target_->FillGeometry(paths_[slot + 2].Get(), gradients_[slot - 4].Get());
                color(rgb);
                target_->DrawGeometry(path.Get(), brush_.Get(), 1.8f, stroke_.Get());
            };
            trend(4, uploadHistory_, 0x61e5b7);
            trend(5, downloadHistory_, 0x32bdeb);
        }
        Check(target_->EndDraw());
        copyPixels(bodyPixels_);
        pixelsDirty_ = true;
        size_ = size;
        return true;
    }
    catch (...)
    {
        // A failed geometry sink may have left an incomplete cached path.
        for (auto& path : paths_) path.Reset();
        ReleaseTarget();
        return false;
    }
}

bool FloatingRateRenderer::Present(HWND window, HWND shadow) noexcept
{
    if (bodyPixels_.empty()) return false;
    if (!surfaceDc_)
    {
        surfaceDc_ = CreateCompatibleDC(nullptr);
        if (!surfaceDc_) return false;
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), size_.cx, -size_.cy, 1, 32, BI_RGB};
        surfaceBitmap_ = CreateDIBSection(surfaceDc_, &info, DIB_RGB_COLORS, &surfaceBits_, nullptr, 0);
        if (!surfaceBitmap_ || !surfaceBits_)
        {
            ReleaseTarget();
            return false;
        }
        surfacePrevious_ = SelectObject(surfaceDc_, surfaceBitmap_);
        if (!surfacePrevious_ || surfacePrevious_ == HGDI_ERROR)
        {
            surfacePrevious_ = nullptr;
            ReleaseTarget();
            return false;
        }
    }
    POINT source{};
    BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
    if (shadow && shadow != presentedShadow_)
    {
        RECT rect{};
        if (!GetWindowRect(window, &rect)) return false;
        POINT position{rect.left, rect.top};
        memcpy(surfaceBits_, shadowPixels_.data(), shadowPixels_.size() * sizeof(DWORD));
        if (!UpdateLayeredWindow(shadow, nullptr, &position, &size_, surfaceDc_, &source, 0, &blend, ULW_ALPHA))
            return false;
        presentedShadow_ = shadow;
    }
    memcpy(surfaceBits_, bodyPixels_.data(), bodyPixels_.size() * sizeof(DWORD));
    return UpdateLayeredWindow(window, nullptr, nullptr, &size_, surfaceDc_, &source, 0, &blend, ULW_ALPHA) != FALSE;
}

const std::vector<DWORD>& FloatingRateRenderer::Pixels() const
{
    if (pixelsDirty_)
    {
        pixels_.resize(bodyPixels_.size());
        for (size_t i = 0; i < pixels_.size(); ++i)
        {
            const DWORD body = bodyPixels_[i];
            const DWORD alpha = (body >> 24) + (shadowPixels_[i] >> 24) * (255 - (body >> 24)) / 255;
            pixels_[i] = (body & 0x00ffffff) | (alpha << 24);
        }
        pixelsDirty_ = false;
    }
    return pixels_;
}
