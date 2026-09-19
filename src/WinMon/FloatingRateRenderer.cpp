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
    for (auto& layout : layouts_) layout.layout.Reset();
    for (auto& path : paths_) path.Reset();
    for (auto& format : formats_) format.Reset();
    stroke_.Reset();
    imagingFactory_.Reset();
    textFactory_.Reset();
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
    brush_.Reset();
    target_.Reset();
    bitmap_.Reset();
    dpi_ = 0;
}

bool FloatingRateRenderer::Render(const std::wstring& upload, const std::wstring& download,
    const LOGFONTW& font, UINT dpi, double pulsePhase, float hoverProgress) noexcept
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
        if (!textFactory_)
            Check(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
                reinterpret_cast<IUnknown**>(textFactory_.ReleaseAndGetAddressOf())));
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
        if (!formats_[0] || memcmp(&font_, &font, sizeof(font)) != 0)
        {
            constexpr float sizes[]{14, 9, 10};
            std::array<ComPtr<IDWriteTextFormat>, 3> formats;
            for (size_t i = 0; i < formats.size(); ++i)
            {
                Check(textFactory_->CreateTextFormat(font.lfFaceName, nullptr,
                    static_cast<DWRITE_FONT_WEIGHT>(font.lfWeight ? font.lfWeight : FW_NORMAL),
                    font.lfItalic ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL,
                    DWRITE_FONT_STRETCH_NORMAL, sizes[i], L"en-us", &formats[i]));
                Check(formats[i]->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP));
                Check(formats[i]->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER));
            }
            for (auto& layout : layouts_) layout.layout.Reset();
            formats_ = std::move(formats);
            font_ = font;
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
        const float activity = static_cast<float>(Activity());
        if (activity > 0)
        {
            const float phase = static_cast<float>(std::isfinite(pulsePhase) ? std::clamp(pulsePhase, 0.0, 1.0) : 0.5);
            const float radius = 2 + 12 * phase;
            const float opacity = (0.2f + 0.8f * activity) * std::sin(phase * 3.14159265f);
            // Soft concentric fills expand from the center; radius + stroke stays
            // below 14.5, safely inside the outer ring's inner radius of 16 DIP.
            for (int layer = 12; layer > 0; --layer)
            {
                const float r = radius * layer / 12;
                color(0x43cfc9, opacity * 0.035f);
                target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(25, 25), r, r), brush_.Get());
            }
            color(0x61e5b7, opacity * 0.4f);
            target_->DrawEllipse(D2D1::Ellipse(D2D1::Point2F(25, 25), radius, radius), brush_.Get(), 0.8f);
        }
        if (count_ > 1)
        {
            const auto trend = [&](size_t slot, const auto& history, UINT32 rgb)
            {
                color(rgb, 0.32f);
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
                    sink->BeginFigure(point(first), D2D1_FIGURE_BEGIN_HOLLOW);
                    for (size_t i = first + 1; i < 48; ++i)
                    {
                        const auto from = point(i - 1), to = point(i);
                        const float third = (to.x - from.x) / 3;
                        sink->AddBezier(D2D1::BezierSegment(
                            D2D1::Point2F(from.x + third, from.y + tangent(i - 1) / 3),
                            D2D1::Point2F(to.x - third, to.y - tangent(i) / 3), to));
                    }
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                    Check(sink->Close());
                }
                target_->DrawGeometry(path.Get(), brush_.Get(), 1.6f, stroke_.Get());
            };
            trend(4, uploadHistory_, 0x61e5b7);
            trend(5, downloadHistory_, 0x32bdeb);
        }
        hoverProgress = std::isfinite(hoverProgress) ? std::clamp(hoverProgress, 0.0f, 1.0f) : 0;
        const float transition = hoverProgress * hoverProgress * (3 - 2 * hoverProgress);
        const auto layoutFor = [&](size_t slot, const std::wstring& value, size_t style,
            float width, float height, bool fit) -> TextLayout&
        {
            auto& cached = layouts_[slot];
            if (!cached.layout || cached.text != value || cached.width != width || cached.height != height)
            {
                ComPtr<IDWriteTextLayout> layout;
                const auto length = static_cast<UINT32>(value.size());
                Check(textFactory_->CreateTextLayout(value.c_str(), length, formats_[style].Get(), width, height, &layout));
                if (fit && font.lfUnderline) Check(layout->SetUnderline(TRUE, {0, length}));
                if (fit && font.lfStrikeOut) Check(layout->SetStrikethrough(TRUE, {0, length}));
                DWRITE_TEXT_METRICS metrics{};
                Check(layout->GetMetrics(&metrics));
                const float scale = std::min({1.0f, width / metrics.widthIncludingTrailingWhitespace, height / metrics.height});
                if (fit && scale < 1)
                {
                    Check(layout->SetFontSize(formats_[style]->GetFontSize() * scale * 0.98f, {0, length}));
                    Check(layout->GetMetrics(&metrics));
                }
                cached = {std::move(layout), value, width, height, metrics.widthIncludingTrailingWhitespace};
            }
            return cached;
        };
        const auto text = [&](size_t slot, const std::wstring& value, size_t style, D2D1_RECT_F rect, UINT32 rgb, float opacity)
        {
            auto& cached = layoutFor(slot, value, style, rect.right - rect.left, style == 2 ? 16.0f : 20.0f, true);
            const auto& layout = cached.layout;
            // A narrow glyph halo keeps overlapping trends from crossing the readings.
            color(0x222e34, opacity);
            for (const auto offset : {D2D1::Point2F(-0.75f, 0), D2D1::Point2F(0.75f, 0),
                D2D1::Point2F(0, -0.75f), D2D1::Point2F(0, 0.75f)})
                target_->DrawTextLayout(D2D1::Point2F(rect.left + offset.x, rect.top + offset.y), layout.Get(), brush_.Get());
            color(rgb, opacity);
            target_->DrawTextLayout(D2D1::Point2F(rect.left, rect.top), layout.Get(), brush_.Get());
            return cached.measuredWidth;
        };
        const auto rateLine = [&](const std::wstring& value, const wchar_t* arrow,
            size_t style, float top, float bottom, UINT32 rgb, float opacity)
        {
            if (opacity <= 0) return;
            const size_t slot = style == 2 ? 0 : 4;
            text(slot, arrow, style, D2D1::RectF(47, top, 58, bottom), rgb, opacity);
            const auto split = value.find(L' ');
            const std::wstring unit = split == std::wstring::npos ? std::wstring{} : value.substr(split);
            const auto& unitLayout = layoutFor(slot + 1, unit, style, 66, style == 2 ? 16.0f : 20.0f, false);
            const float width = text(slot + 2, value.substr(0, split), style,
                D2D1::RectF(59, top, 113 - unitLayout.measuredWidth, bottom), rgb, opacity);
            text(slot + 3, unit, style, D2D1::RectF(59 + width, top, 113, bottom), rgb, opacity);
        };
        rateLine(upload, L"\u2191", 2, 7, 23, 0x61e5b7, transition);
        rateLine(download, L"\u2193", 0, 15 + 8 * transition, 35 + 8 * transition, 0x32bdeb, 1);
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
