// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include "floating/PunctumGlyphs.h"
#include <d2d1.h>
#include <array>
#include <string>
#include <algorithm>
#include <cmath>

namespace led
{
inline bool Cells(const std::wstring& text, wchar_t arrow, std::array<wchar_t, 9>& out)
{
    const auto dot = text.find(L'.');
    if (dot == std::wstring::npos || dot == 0 || text.size() != dot + 6 ||
        text[dot + 1] < L'0' || text[dot + 1] > L'9' || text[dot + 2] != L' ' ||
        (text[dot + 3] != L'K' && text[dot + 3] != L'M' && text[dot + 3] != L'G') ||
        text.substr(dot + 4) != L"/s") return false;
    for (size_t i = 0; i < dot; ++i) if (text[i] < L'0' || text[i] > L'9') return false;
    out = {arrow, L' ', L' ', L' ', L'.', text[dot + 1], text[dot + 3], L'/', L's'};
    const auto first = text.find_first_not_of(L'0');
    const size_t digits = first < dot ? dot - first : 1;
    if (digits > 3) out[1] = out[2] = out[3] = out[5] = L'9';
    else for (size_t i = 0; i < digits; ++i) out[4 - digits + i] = text[dot - digits + i];
    return true;
}

inline void Draw(ID2D1RenderTarget* target, ID2D1SolidColorBrush* brush,
    const std::array<wchar_t, 9>& characters, const std::array<wchar_t, 9>& previous,
    float x, float baseline, UINT32 rgb, float opacity = 1, float progress = 1, bool lightTheme = false, bool arrowOnRight = false, bool litOnly = false)
{
    for (size_t cell = 0; cell < characters.size(); ++cell)
    {
        const size_t source = arrowOnRight ? (cell + 1) % 9 : cell;
        if (litOnly && arrowOnRight && source == 0) x += 4;
        if (litOnly && !arrowOnRight && source == 1) x += 3.2f;
        const float pitch = source < 6 ? 1.05f : 0.8f;
        const bool changing = characters[source] != previous[source];
        const auto* glyph = punctum::Find(changing && progress < 0.5f ? previous[source] : characters[source]);
        const float lamp = changing ? std::abs(2 * progress - 1) : 1;
        const bool decimal = characters[source] == L'.';
        const int columns = decimal ? 3 : 5;
        for (int row = 0; row < 7; ++row)
            for (int column = 0; column < columns; ++column)
            {
                const bool lit = glyph && (glyph->rows[row] & (1 << (column + (decimal ? 1 : 0))));
                if (litOnly && !lit) continue;
                const UINT32 digitColor = rgb == 0x61e5b7 ? 0x9affd4 : 0x80e4ff;
                const auto dark = D2D1::ColorF(lightTheme ? 0xb4bfc4 : 0x34424a, opacity);
                auto color = D2D1::ColorF(lightTheme ? (source < 6 ? (rgb == 0x61e5b7 ? 0x006b49 : 0x00668b) : 0x405560)
                    : (source == 0 ? rgb : source < 6 ? digitColor : 0x849da9), opacity);
                const float intensity = lit ? lamp : 0;
                color.r = dark.r + (color.r - dark.r) * intensity;
                color.g = dark.g + (color.g - dark.g) * intensity;
                color.b = dark.b + (color.b - dark.b) * intensity;
                if (litOnly)
                    color = D2D1::ColorF(source == 0 ? (lightTheme ? (rgb == 0x61e5b7 ? 0x003923 : 0x003452) : rgb)
                        : source < 6 ? (lightTheme ? (rgb == 0x61e5b7 ? 0x003923 : 0x003452) : digitColor)
                        : (lightTheme ? 0x18212a : 0x9aa5af), opacity * lamp);
                brush->SetColor(color);
                const float radius = pitch * (litOnly ? (lightTheme ? 0.46f : 0.4f) : 0.36f);
                target->FillEllipse(D2D1::Ellipse(D2D1::Point2F(x + column * pitch, baseline - (6 - row) * pitch),
                    radius, radius), brush);
            }
        x += (columns - 1) * pitch + ((source == 0 || source == 5) ? 2 * 1.05f : 1.5f * pitch);
    }
}
}
