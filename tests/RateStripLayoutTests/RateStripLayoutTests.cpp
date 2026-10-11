#include "taskbar/RateStripLayout.h"
#include <iostream>
#include <stdexcept>
#include <vector>

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try
    {
        HDC dc = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), 1024, -256, 1, 32, BI_RGB};
        void* actualBits = nullptr;
        HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &actualBits, nullptr, 0);
        auto oldBitmap = SelectObject(dc, bitmap);
        HDC reference = CreateCompatibleDC(nullptr);
        void* referenceBits = nullptr;
        HBITMAP referenceBitmap = CreateDIBSection(reference, &info, DIB_RGB_COLORS, &referenceBits, nullptr, 0);
        auto oldReferenceBitmap = SelectObject(reference, referenceBitmap);
        for (const auto* face : {L"Segoe UI", L"Arial", L"Consolas"})
        for (int dpi : {96, 144, 192})
        for (int points : {6, 12})
        for (bool italic : {false, true})
        {
            HFONT font = CreateFontW(-MulDiv(points, dpi, 72), 0, 0, 0,
                italic ? FW_BOLD : FW_NORMAL, italic, FALSE, FALSE, DEFAULT_CHARSET,
                OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY,
                DEFAULT_PITCH, face);
            auto oldFont = SelectObject(dc, font);
            auto oldReferenceFont = SelectObject(reference, font);
            RateStripLayout layout;
            Require(layout.Measure(dc, dpi), "active font can be measured");
            for (bool left : {false, true})
            for (const auto* unit : {L"K/s", L"M/s", L"G/s"})
            for (bool bottom : {false, true})
            {
                const int top = bottom ? layout.height / 2 : 0;
                const int end = bottom ? layout.height : layout.height / 2;
                const auto columns = layout.Columns(left, top, end);
                const auto* arrow = bottom ? L"\u2193" : L"\u2191";
                Require(left ? columns.arrow.right < columns.number.left
                             : columns.unit.right < columns.arrow.left, "arrow order matches side");
                std::vector<COLORREF> unitPixels, arrowPixels;
                auto pixels = [&](HDC source, RECT rect) {
                    GdiFlush();
                    const auto* bits = static_cast<const DWORD*>(source == dc ? actualBits : referenceBits);
                    std::vector<COLORREF> result;
                    for (int y = rect.top; y < rect.bottom; ++y)
                    for (int x = rect.left; x < rect.right; ++x) result.push_back(bits[y * 1024 + x]);
                    return result;
                };
                for (const auto* number : {L"0.0", L"9.9", L"999.9", L"1000.0", L"123456789.0"})
                {
                    PatBlt(dc, 0, 0, 1024, 256, BLACKNESS);
                    SetTextColor(dc, RGB(255, 255, 255));
                    SetBkMode(dc, TRANSPARENT);
                    Require(layout.Draw(dc, std::wstring(number) + L" " + unit, arrow, columns), "reading draws");
                    auto units = pixels(dc, columns.unit), arrows = pixels(dc, columns.arrow);
                    if (unitPixels.empty()) { unitPixels = units; arrowPixels = arrows; }
                    Require(units == unitPixels && arrows == arrowPixels, "units and arrows remain stationary across rate lengths and overflow");
                    Require(std::any_of(units.begin(), units.end(), [](auto pixel) { return pixel != RGB(0, 0, 0); }), "unit is rendered");
                    Require(std::any_of(arrows.begin(), arrows.end(), [](auto pixel) { return pixel != RGB(0, 0, 0); }), "arrow is rendered");
                    PatBlt(reference, 0, 0, 1024, 256, BLACKNESS);
                    SetTextColor(reference, RGB(255, 255, 255));
                    SetBkMode(reference, TRANSPARENT);
                    auto expected = columns;
                    const auto* expectedNumber = wcscmp(number, L"123456789.0") == 0 ? L"\u2026" : number;
                    constexpr UINT flags = DT_SINGLELINE | DT_VCENTER | DT_NOPREFIX | DT_NOCLIP;
                    DrawTextW(reference, expectedNumber, -1, &expected.number, flags | DT_RIGHT);
                    DrawTextW(reference, unit, -1, &expected.unit, flags | DT_LEFT);
                    DrawTextW(reference, arrow, -1, &expected.arrow, flags | DT_LEFT);
                    const RECT row{0, top, layout.width, end};
                    if (pixels(dc, row) != pixels(reference, row))
                    {
                        std::wcerr << face << L" dpi=" << dpi << L" points=" << points << L" italic=" << italic << L" left=" << left << L" unit=" << unit << L" number=" << number << L'\n';
                        Require(false, "complete glyphs match unclipped reference, including explicit overflow ellipsis");
                    }
                }
                SIZE capacity{};
                GetTextExtentPoint32W(dc, L"1000.0", 6, &capacity);
                Require(columns.number.right - columns.number.left >= capacity.cx, "rounding boundary fits");
            }
            SelectObject(dc, oldFont);
            SelectObject(reference, oldReferenceFont);
            DeleteObject(font);
        }
        SelectObject(dc, oldBitmap);
        DeleteObject(bitmap);
        DeleteDC(dc);
        SelectObject(reference, oldReferenceBitmap);
        DeleteObject(referenceBitmap);
        DeleteDC(reference);
        std::cout << "RateStripLayoutTests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
