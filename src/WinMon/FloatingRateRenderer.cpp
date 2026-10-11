#include "FloatingRateRenderer.h"
#include "PunctumGlyphs.h"
#include <algorithm>
#include <cmath>

namespace
{
constexpr float kRingDiameter = 24;
constexpr float kChartLeft = 35;
constexpr float kChartWidth = kRingDiameter * 8 / 5;
// The original ring was 37 DIP before its 12/11 body transform.
constexpr float kContentScale = (37.0f * 12 / 11) / kRingDiameter;
constexpr float kRingStroke = (2.5f * 12 / 11) / kContentScale;
constexpr float kRingRadius = (kRingDiameter - kRingStroke) / 2;
float BodyRight(float progress) noexcept
{
    const float chartRight = 27 + (kChartLeft + kChartWidth - 20) * kContentScale;
    const float textRight = 27 + (126 - 2 * 1.05f - 20) * kContentScale;
    return chartRight + 15 + 8 * (1 - progress) + (textRight - chartRight) * progress;
}

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

SIZE FloatingRateRenderer::SizeForDpi(UINT dpi, float textProgress) noexcept
{
    return {static_cast<LONG>(std::lround((BodyRight(std::clamp(textProgress, 0.0f, 1.0f)) + 3) * dpi / 96)), MulDiv(54, static_cast<int>(dpi), 96)};
}

bool FloatingRateRenderer::HitTest(POINT client, UINT dpi, float textProgress) noexcept
{
    if (!dpi) return false;
    const float x = client.x * 96.0f / dpi - 3;
    const float y = client.y * 96.0f / dpi - 27;
    const float dx = x - std::clamp(x, 24.0f, BodyRight(std::clamp(textProgress, 0.0f, 1.0f)) - 27);
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
    for (size_t i = 32 - count_; i < 32; ++i)
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

bool FloatingRateRenderer::Render(const std::wstring& upload, const std::wstring& download,
    UINT dpi, double pulsePhase, float waterLevel, float textProgress, const LedReadoutTransition* transition) noexcept
{
    try
    {
        if (dpi < 48 || dpi > 768 || !std::isfinite(textProgress) || textProgress < 0 || textProgress > 1) return false;
        const auto cells = [](const std::wstring& text, wchar_t arrow, std::array<wchar_t, 9>& out) {
            const auto dot = text.find(L'.');
            if (dot == std::wstring::npos || dot == 0 || text.size() != dot + 6 ||
                text[dot + 1] < L'0' || text[dot + 1] > L'9' || text[dot + 2] != L' ' ||
                (text[dot + 3] != L'K' && text[dot + 3] != L'M' && text[dot + 3] != L'G') ||
                text.substr(dot + 4) != L"/s") return false;
            for (size_t i = 0; i < dot; ++i) if (text[i] < L'0' || text[i] > L'9') return false;
            out = {arrow, L' ', L' ', L' ', L'.', text[dot + 1], text[dot + 3], L'/', L's'};
            const auto first = text.find_first_not_of(L'0');
            const size_t digits = first < dot ? dot - first : 1;
            if (digits > 3) { out[1] = out[2] = out[3] = out[5] = L'9'; }
            else for (size_t i = 0; i < digits; ++i) out[4 - digits + i] = text[dot - digits + i];
            return true;
        };
        std::array<wchar_t, 9> upCells{}, downCells{};
        if (!cells(upload, L'\u2191', upCells) || !cells(download, L'\u2193', downCells)) return false;
        auto oldUpCells = upCells, oldDownCells = downCells;
        const float lampProgress = transition ? std::clamp(transition->progress, 0.0f, 1.0f) : 1;
        if (transition)
        {
            if (!cells(transition->upload, L'\u2191', oldUpCells)) oldUpCells = upCells;
            if (!cells(transition->download, L'\u2193', oldDownCells)) oldDownCells = downCells;
        }
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
        const SIZE size = SizeForDpi(dpi, textProgress);
        const float right = BodyRight(textProgress);
        if (!target_ || dpi_ != dpi || size.cx != size_.cx || size.cy != size_.cy)
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
            // The shadow is cached until DPI or canvas size changes.
            color(0x000000, 0.06f);
            target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(1, 1, right + 2, 53), 26, 26), brush_.Get());
            color(0x000000, 0.10f);
            target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(2, 2, right + 1, 52), 25, 25), brush_.Get());
            Check(target_->EndDraw());
            copyPixels(shadowPixels_);
        }
        target_->BeginDraw();
        target_->Clear(D2D1::ColorF(0, 0.0f));
        // All geometry uses DIP; the render target performs DPI scaling.
        target_->SetTransform(D2D1::Matrix3x2F::Identity());
        color(0x222e34);
        target_->FillRoundedRectangle(D2D1::RoundedRect(D2D1::RectF(3, 3, right, 51), 24, 24), brush_.Get());
        target_->SetTransform(D2D1::Matrix3x2F::Scale(kContentScale, kContentScale, D2D1::Point2F(20, 27)) *
            D2D1::Matrix3x2F::Translation(7, 0));
        double maximum = 1000;
        for (size_t i = 32 - count_; i < 32; ++i)
            maximum = std::max({maximum, uploadHistory_[i], downloadHistory_[i]});
        const auto arc = [&](size_t slot, double ratio, bool up, UINT32 rgb)
        {
            if (ratio <= 0) return;
            // Leave about 2 DIP of clear space after accounting for the round caps.
            constexpr double gapAngle = 0.13;
            const double sweep = std::clamp(ratio, 0.0, 1.0) * (3.141592653589793 - 2 * gapAngle);
            auto& path = paths_[slot];
            if (!path)
            {
                ComPtr<ID2D1GeometrySink> sink;
                Check(factory_->CreatePathGeometry(&path));
                Check(path->Open(&sink));
                const auto point = [&](double angle) {
                    return D2D1::Point2F(20 + kRingRadius * static_cast<float>(std::cos(angle)),
                        27 + (up ? -kRingRadius : kRingRadius) * static_cast<float>(std::sin(angle)));
                };
                const auto section = [&](double start, double end, bool reverse) {
                    sink->BeginFigure(point(start), D2D1_FIGURE_BEGIN_HOLLOW);
                    sink->AddArc(D2D1::ArcSegment(point(end), D2D1::SizeF(kRingRadius, kRingRadius), 0,
                        (up != reverse) ? D2D1_SWEEP_DIRECTION_COUNTER_CLOCKWISE : D2D1_SWEEP_DIRECTION_CLOCKWISE,
                        D2D1_ARC_SIZE_SMALL));
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                };
                if (ratio >= 1) section(gapAngle, 3.141592653589793 - gapAngle, false);
                else
                {
                    section(gapAngle, gapAngle + sweep / 2, false);
                    section(3.141592653589793 - gapAngle, 3.141592653589793 - gapAngle - sweep / 2, true);
                }
                Check(sink->Close());
            }
            color(rgb);
            target_->DrawGeometry(path.Get(), brush_.Get(), kRingStroke, stroke_.Get());
        };
        arc(0, 1, true, 0x46565f);
        arc(1, 1, false, 0x46565f);
        arc(2, upload_ > 0 ? uploadHistory_.back() / maximum : 0, true, 0x61e5b7);
        arc(3, download_ > 0 ? downloadHistory_.back() / maximum : 0, false, 0x32bdeb);
        const float level = std::isfinite(waterLevel) && waterLevel >= 0
            ? std::clamp(waterLevel, 0.0f, 1.0f) : (Activity() > 0 ? 0.15f + 0.65f * static_cast<float>(Activity()) : 0);
        if (level > 0)
        {
            const float phase = static_cast<float>(std::isfinite(pulsePhase) ? pulsePhase : 0.5);
            ComPtr<ID2D1EllipseGeometry> circle;
            Check(factory_->CreateEllipseGeometry(D2D1::Ellipse(D2D1::Point2F(20, 27), 8.5f, 8.5f), &circle));
            ComPtr<ID2D1PathGeometry> water, surface;
            ComPtr<ID2D1GeometrySink> surfaceSink;
            Check(factory_->CreatePathGeometry(&surface));
            Check(surface->Open(&surfaceSink));
            ComPtr<ID2D1GeometrySink> sink;
            Check(factory_->CreatePathGeometry(&water));
            Check(water->Open(&sink));
            sink->BeginFigure(D2D1::Point2F(11, 36), D2D1_FIGURE_BEGIN_FILLED);
            const float amplitude = 1.2f * std::sin(level * 3.14159265f);
            for (int x = 11; x <= 29; ++x)
            {
                const auto point = D2D1::Point2F(static_cast<float>(x), 35.5f - 17 * level +
                    amplitude * std::sin((x - 11) * 0.4f + phase * 6.2831853f));
                sink->AddLine(point);
                if (x == 11) surfaceSink->BeginFigure(point, D2D1_FIGURE_BEGIN_HOLLOW);
                else surfaceSink->AddLine(point);
            }
            surfaceSink->EndFigure(D2D1_FIGURE_END_OPEN);
            Check(surfaceSink->Close());
            sink->AddLine(D2D1::Point2F(29, 36));
            sink->EndFigure(D2D1_FIGURE_END_CLOSED);
            Check(sink->Close());
            target_->PushLayer(D2D1::LayerParameters(D2D1::InfiniteRect(), circle.Get()), nullptr);
            target_->FillGeometry(water.Get(), gradients_[2].Get());
            // Soft concentric strokes follow only the waterline, clipped inside the ring.
            color(0x79efe5, 0.06f);
            target_->DrawGeometry(surface.Get(), brush_.Get(), 3.0f, stroke_.Get());
            color(0x79efe5, 0.12f);
            target_->DrawGeometry(surface.Get(), brush_.Get(), 1.6f, stroke_.Get());
            color(0xb4fff2, 0.45f);
            target_->DrawGeometry(surface.Get(), brush_.Get(), 0.45f, stroke_.Get());
            target_->PopLayer();
        }
        {
            const auto trend = [&](size_t slot, const auto& history, UINT32 rgb)
            {
                const float top = slot == 4 ? 15.0f : 27.75f;
                const float bottom = slot == 4 ? 26.25f : 39.0f;
                const auto clip = D2D1::RectF(kChartLeft, top, kChartLeft + kChartWidth, bottom);
                target_->PushAxisAlignedClip(clip, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
                color(0x93afb9, 0.08f);
                for (int column = 0; column <= 10; ++column)
                {
                    const float x = kChartLeft + kChartWidth * column / 10;
                    target_->DrawLine(D2D1::Point2F(x, top), D2D1::Point2F(x, bottom), brush_.Get(), 0.5f);
                }
                for (int row = 0; row <= 3; ++row)
                {
                    const float y = top + (bottom - top) * row / 3;
                    target_->DrawLine(D2D1::Point2F(kChartLeft, y), D2D1::Point2F(kChartLeft + kChartWidth, y), brush_.Get(), 0.5f);
                }
                if (count_ < 2) { target_->PopAxisAlignedClip(); return; }

                auto& path = paths_[slot];
                if (!path)
                {
                    const auto point = [&](size_t i)
                    {
                        return D2D1::Point2F(kChartLeft + kChartWidth * static_cast<float>(i) / 31,
                            bottom - (bottom - top) * static_cast<float>(history[i] / maximum));
                    };
                    const size_t first = 32 - count_;
                    const auto tangent = [&](size_t i)
                    {
                        if (i == first) return point(i + 1).y - point(i).y;
                        if (i == 31) return point(i).y - point(i - 1).y;
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
                    for (size_t i = first + 1; i < 32; ++i)
                    {
                        const auto from = point(i - 1), to = point(i);
                        const float third = (to.x - from.x) / 3;
                        const auto segment = D2D1::BezierSegment(
                            D2D1::Point2F(from.x + third, from.y + tangent(i - 1) / 3),
                            D2D1::Point2F(to.x - third, to.y - tangent(i) / 3), to);
                        sink->AddBezier(segment);
                        fillSink->AddBezier(segment);
                    }
                    fillSink->AddLine(D2D1::Point2F(kChartLeft + kChartWidth, bottom));
                    fillSink->AddLine(D2D1::Point2F(point(first).x, bottom));
                    fillSink->EndFigure(D2D1_FIGURE_END_CLOSED);
                    Check(fillSink->Close());
                    sink->EndFigure(D2D1_FIGURE_END_OPEN);
                    Check(sink->Close());
                }
                gradients_[slot - 4]->SetStartPoint(D2D1::Point2F(0, top));
                gradients_[slot - 4]->SetEndPoint(D2D1::Point2F(0, bottom));
                target_->FillGeometry(paths_[slot + 2].Get(), gradients_[slot - 4].Get());
                color(rgb);
                target_->DrawGeometry(path.Get(), brush_.Get(), 1.0f, stroke_.Get());
                target_->PopAxisAlignedClip();
            };
            trend(4, uploadHistory_, 0x61e5b7);
            trend(5, downloadHistory_, 0x32bdeb);
        }
        target_->PushAxisAlignedClip(D2D1::RectF(std::min(77 - 0.4f, 20 + (right - 15 - 27) / kContentScale), 15, 20 + (right - 15 - 27) / kContentScale, 39), D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
        const auto readout = [&](const auto& characters, const auto& previous, float baseline, UINT32 rgb) {
            float x = 77;
            for (size_t cell = 0; cell < characters.size(); ++cell)
            {
                const float pitch = cell < 6 ? 1.05f : 0.8f;
                const bool changing = characters[cell] != previous[cell];
                const auto* glyph = punctum::Find(changing && lampProgress < 0.5f ? previous[cell] : characters[cell]);
                const float lamp = changing ? std::abs(2 * lampProgress - 1) : 1;
                const bool decimal = characters[cell] == L'.';
                const int columns = decimal ? 3 : 5;
                for (int row = 0; row < 7; ++row)
                    for (int column = 0; column < columns; ++column)
                    {
                        const bool lit = glyph && (glyph->rows[row] & (1 << (column + (decimal ? 1 : 0))));
                        const UINT32 digitColor = rgb == 0x61e5b7 ? 0x9affd4 : 0x80e4ff;
                        const auto dark = D2D1::ColorF(0x34424a, textProgress);
                        auto light = D2D1::ColorF(cell == 0 ? rgb : cell < 6 ? digitColor : 0x849da9, textProgress);
                        const float intensity = lit ? lamp : 0;
                        light.r = dark.r + (light.r - dark.r) * intensity;
                        light.g = dark.g + (light.g - dark.g) * intensity;
                        light.b = dark.b + (light.b - dark.b) * intensity;
                        brush_->SetColor(light);
                        target_->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + column * pitch, baseline - (6 - row) * pitch),
                            pitch * 0.36f, pitch * 0.36f), brush_.Get());
                    }
                x += (columns - 1) * pitch + ((cell == 0 || cell == 5) ? 2 * 1.05f : 1.5f * pitch);
            }
        };
        if (textProgress > 0) readout(upCells, oldUpCells, 23.8f, 0x61e5b7);
        if (textProgress > 0) readout(downCells, oldDownCells, 36.55f, 0x32bdeb);
        target_->PopAxisAlignedClip();
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
