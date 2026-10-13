// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include "floating/LedReadout.h"
#include <wrl/client.h>

namespace ledStrip
{
inline SIZE Size(UINT dpi) noexcept { return {MulDiv(94, dpi, 96), MulDiv(36, dpi, 96)}; }
inline bool Render(HDC dc, SIZE size, const std::wstring& upload, const std::wstring& download, bool left, bool light)
{
    using Microsoft::WRL::ComPtr;
    ComPtr<ID2D1Factory> factory;
    ComPtr<ID2D1DCRenderTarget> target;
    ComPtr<ID2D1SolidColorBrush> brush;
    std::array<wchar_t, 9> up{}, down{};
    const RECT bounds{0, 0, size.cx, size.cy};
    const auto properties = D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_SOFTWARE,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    bool rendered = led::Cells(upload, L'\u2191', up) && led::Cells(download, L'\u2193', down) &&
        SUCCEEDED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, factory.GetAddressOf())) &&
        SUCCEEDED(factory->CreateDCRenderTarget(&properties, target.GetAddressOf())) &&
        SUCCEEDED(target->BindDC(dc, &bounds)) &&
        SUCCEEDED(target->CreateSolidColorBrush(D2D1::ColorF(0xffffff), brush.GetAddressOf()));
    if (rendered)
    {
        target->BeginDraw();
        target->Clear(D2D1::ColorF(0, 0));
        target->SetTransform(D2D1::Matrix3x2F::Scale(size.cx / 55.0f, size.cy / 22.0f));
        led::Draw(target.Get(), brush.Get(), up, up, 2, 8, 0x61e5b7, 1, 1, light, !left, true);
        led::Draw(target.Get(), brush.Get(), down, down, 2, 19, 0x32bdeb, 1, 1, light, !left, true);
        rendered = SUCCEEDED(target->EndDraw());
    }
    return rendered;
}
}
