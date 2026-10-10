#pragma once
#include <windows.h>
#include <algorithm>
#include <string>

struct RateStripColumns final
{
    RECT number{}, unit{}, arrow{};
};

// Shared measurement and drawing keep the two rows' anchors independent of their readings.
struct RateStripLayout final
{
    int width = 0, height = 0;
    int numberWidth = 0, unitWidth = 0, arrowWidth = 0, gap = 0, padding = 0;
    int glyphPadding = 0;

    bool Measure(HDC dc, UINT dpi) noexcept
    {
        TEXTMETRICW metrics{};
        if (!GetTextMetricsW(dc, &metrics)) return false;
        auto extent = [dc](const wchar_t* text) {
            SIZE size{};
            return GetTextExtentPoint32W(dc, text, static_cast<int>(wcslen(text)), &size) ? size.cx : -1;
        };
        int digitWidth = 0;
        for (wchar_t digit = L'0'; digit <= L'9'; ++digit)
        {
            const wchar_t text[]{digit, 0};
            const int measured = extent(text);
            if (measured < 0) return false;
            digitWidth = std::max(digitWidth, measured);
        }
        const int decimalWidth = extent(L"."), spaceWidth = extent(L" ");
        const int kiloWidth = extent(L"K/s"), megaWidth = extent(L"M/s"), gigaWidth = extent(L"G/s");
        const int upWidth = extent(L"\u2191"), downWidth = extent(L"\u2193");
        if (std::min({decimalWidth, spaceWidth, kiloWidth, megaWidth, gigaWidth, upWidth, downWidth}) < 0) return false;
        glyphPadding = metrics.tmOverhang + 2;
        for (const wchar_t glyph : std::wstring(L"0123456789.KMG/s\u2191\u2193\u2026"))
        {
            ABC bearing{};
            if (GetCharABCWidthsW(dc, glyph, glyph, &bearing))
                glyphPadding = std::max({glyphPadding, -bearing.abcA, -bearing.abcC});
        }
        numberWidth = digitWidth * 5 + decimalWidth + glyphPadding * 2;
        unitWidth = std::max({kiloWidth, megaWidth, gigaWidth}) + glyphPadding * 2;
        arrowWidth = std::max(upWidth, downWidth) + glyphPadding * 2;
        gap = std::max(spaceWidth, MulDiv(1, static_cast<int>(dpi), 96));
        padding = MulDiv(8, static_cast<int>(dpi), 96);
        width = padding * 2 + numberWidth + unitWidth + arrowWidth + gap * 2;
        height = metrics.tmHeight * 2 + MulDiv(6, static_cast<int>(dpi), 96);
        return width > 0 && height > 0;
    }

    RateStripColumns Columns(bool left, int top, int bottom) const noexcept
    {
        const int numberX = padding + (left ? arrowWidth + gap : 0);
        const int unitX = numberX + numberWidth + gap;
        const int arrowX = left ? padding : unitX + unitWidth + gap;
        // Drawing anchors leave negative glyph bearings inside each reserved cell.
        return {{numberX + glyphPadding, top, numberX + numberWidth - glyphPadding, bottom},
                {unitX + glyphPadding, top, unitX + unitWidth - glyphPadding, bottom},
                {arrowX + glyphPadding, top, arrowX + arrowWidth - glyphPadding, bottom}};
    }

    bool Draw(HDC dc, const std::wstring& reading, const wchar_t* arrow, RateStripColumns columns) const noexcept
    {
        const auto split = reading.rfind(L' ');
        if (split == std::wstring::npos) return false;
        auto number = reading.substr(0, split);
        const auto unit = reading.substr(split + 1);
        SIZE actual{};
        if (!GetTextExtentPoint32W(dc, number.c_str(), static_cast<int>(number.size()), &actual)) return false;
        if (actual.cx > columns.number.right - columns.number.left) number = L"\u2026";
        constexpr UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP;
        return DrawTextW(dc, number.c_str(), -1, &columns.number, flags | DT_RIGHT) != 0 &&
            DrawTextW(dc, unit.c_str(), -1, &columns.unit, flags | DT_LEFT) != 0 &&
            DrawTextW(dc, arrow, -1, &columns.arrow, flags | DT_LEFT) != 0;
    }
};
