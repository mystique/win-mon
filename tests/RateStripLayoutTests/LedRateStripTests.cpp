#include "taskbar/RateStrip.h"
#include "floating/LedReadout.h"
#include "WinMonCore.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    try
    {
        std::array<wchar_t, 9> cells{}, saturated{};
        Require(led::Cells(L"999.9 G/s", L'\u2191', saturated), "capacity parses");
        Require(led::Cells(L"1000.0 G/s", L'\u2191', cells) && cells == saturated, "overflow saturates in selected unit");
        Require(led::Cells(L"0000.0 K/s", L'\u2191', cells) && cells[1] == L' ' && cells[3] == L'0', "leading cells are blank");
        Require(!led::Cells(L"12.x K/s", L'\u2191', cells) && !led::Cells(L"12.0 B/s", L'\u2191', cells), "malformed readouts rejected");
        Require(winmon::WinMonCore::FormatRateCells(0.0, L'\u2191') == std::array<wchar_t, 9>{L'\u2191', L' ', L' ', L'0', L'.', L'0', L'K', L'/', L's'}, "numeric zero cells");
        Require(winmon::WinMonCore::FormatRateCells(12345.0, L'\u2193') == std::array<wchar_t, 9>{L'\u2193', L' ', L'1', L'2', L'.', L'3', L'K', L'/', L's'}, "numeric K cells");
        Require(winmon::WinMonCore::FormatRateCells(999999999.0, L'\u2191') == std::array<wchar_t, 9>{L'\u2191', L'9', L'9', L'9', L'.', L'9', L'M', L'/', L's'}, "numeric rounding overflow saturates in unit");
        RateStrip strip;
        for (UINT dpi : {96u, 120u, 144u, 192u})
        for (bool left : {false, true})
        {
            Require(strip.SetTaskbarDisplayOnLeft(left), "configure strip side");
            const SIZE size = RateStrip::SizeForDpi(dpi);
            HDC dc = CreateCompatibleDC(nullptr);
            BITMAPINFO info{};
            info.bmiHeader = {sizeof(BITMAPINFOHEADER), size.cx, -size.cy, 1, 32, BI_RGB};
            void* bits = nullptr;
            HBITMAP bitmap = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
            Require(dc && bitmap && bits, "test surface");
            auto previous = SelectObject(dc, bitmap);
            auto* pixels = static_cast<DWORD*>(bits);
            std::vector<DWORD> dark;
            for (bool light : {false, true})
            {
                strip.SetRates(L"999.9 G/s", L"12.8 M/s");
                Require(strip.RenderToDc(dc, size, light), "DPI/theme frame renders");
                const size_t count = static_cast<size_t>(size.cx) * size.cy;
                Require(std::count_if(pixels, pixels + count, [](DWORD p) { return (p >> 24) != 0; }) > static_cast<std::ptrdiff_t>(count / 10), "lit lamps occupy both readouts");
                for (bool upload : {true, false})
                {
                    const int top = upload ? 0 : size.cy / 2;
                    const int bottom = upload ? size.cy / 2 : size.cy;
                    bool coloredNumber = false;
                    for (int y = top; y < bottom; ++y)
                        for (int x = size.cx / 3; x < size.cx / 2; ++x)
                        {
                            const DWORD p = pixels[y * size.cx + x];
                            const int g = (p >> 8) & 255, b = p & 255;
                            if ((p >> 24) > 160 && (upload ? g > b + 15 : b > g + 15)) coloredNumber = true;
                        }
                    Require(coloredNumber, "numeric columns distinguish green upload and blue download in both themes");
                }
                for (int x = 0; x < size.cx; ++x)
                    Require(pixels[x] == 0 && pixels[(size.cy - 1) * size.cx + x] == 0, "vertical lamps not clipped");
                for (int y = 0; y < size.cy; ++y)
                    Require(pixels[y * size.cx] == 0 && pixels[y * size.cx + size.cx - 1] == 0, "horizontal lamps not clipped");
                for (size_t i = 0; i < count; ++i)
                    Require(((pixels[i] >> 16) & 255) <= (pixels[i] >> 24) &&
                        ((pixels[i] >> 8) & 255) <= (pixels[i] >> 24) && (pixels[i] & 255) <= (pixels[i] >> 24), "premultiplied alpha");
                if (!light) dark.assign(pixels, pixels + count);
                else Require(!std::equal(dark.begin(), dark.end(), pixels), "light theme changes palette");
                if (argc > 1)
                {
                    const std::filesystem::path dir(argv[1]);
                    std::filesystem::create_directories(dir);
                    auto path = dir / (std::to_string(dpi) + (left ? "-left" : "-right") + (light ? "-light.bmp" : "-dark.bmp"));
                    BITMAPFILEHEADER file{};
                    file.bfType = 0x4d42;
                    file.bfOffBits = sizeof(file) + sizeof(info.bmiHeader);
                    file.bfSize = static_cast<DWORD>(file.bfOffBits + count * 4);
                    std::ofstream out(path, std::ios::binary);
                    out.write(reinterpret_cast<const char*>(&file), sizeof(file));
                    out.write(reinterpret_cast<const char*>(&info.bmiHeader), sizeof(info.bmiHeader));
                    const DWORD background = light ? 0xf3f3f3 : 0x202020;
                    for (size_t i = 0; i < count; ++i)
                    {
                        DWORD p = 0xff000000;
                        for (int shift : {0, 8, 16}) p |= (((pixels[i] >> shift) & 255) +
                            ((background >> shift) & 255) * (255 - (pixels[i] >> 24)) / 255) << shift;
                        out.write(reinterpret_cast<const char*>(&p), sizeof(p));
                    }
                    Require(out.good(), "evidence saved");
                }
            }
            Require(strip.RenderToDc(dc, size, false), "switch back to dark theme");
            Require(std::equal(dark.begin(), dark.end(), pixels), "dark-light-dark restores the exact original frame");
            strip.SetRates(L"1.8 K/s", L"1.8 K/s");
            Require(strip.RenderToDc(dc, size, false), "short readouts render");
            if (!left)
            {
                const int gapLeft = static_cast<int>(std::ceil(42.0f * size.cx / 55));
                const int gapRight = static_cast<int>(46.0f * size.cx / 55);
                bool nearRightEdge = false;
                for (int y = 0; y < size.cy; ++y)
                {
                    for (int x = gapLeft; x < gapRight; ++x)
                        Require(pixels[y * size.cx + x] == 0, "unit and right arrow have clear separation");
                    for (int x = size.cx - MulDiv(9, dpi, 96); x < size.cx; ++x)
                        nearRightEdge |= (pixels[y * size.cx + x] >> 24) != 0;
                }
                Require(nearRightEdge, "right arrow leaves no excessive trailing padding");
            }
            const float blankX = left ? 11.5f : 2.0f;
            const int blankLeft = static_cast<int>((blankX - 0.42f) * size.cx / 55);
            const int blankRight = static_cast<int>(std::ceil((blankX + 4.2f + 0.42f) * size.cx / 55));
            for (int y = 0; y < size.cy; ++y)
                for (int x = blankLeft; x < blankRight; ++x)
                    Require(pixels[y * size.cx + x] == 0, "leading cell has no dark-lamp grid");
            if (left)
            {
                strip.SetRates(L"888.8 K/s", L"888.8 K/s");
                Require(strip.RenderToDc(dc, size, false), "full left numeric capacity renders");
                const int gapLeft = static_cast<int>(std::ceil(7.0f * size.cx / 55));
                const int gapRight = static_cast<int>(11.0f * size.cx / 55);
                for (int y = 0; y < size.cy; ++y)
                    for (int x = gapLeft; x < gapRight; ++x)
                        Require(pixels[y * size.cx + x] == 0, "left arrow and nearest numeric cell have clear separation");
            }
            SelectObject(dc, previous);
            DeleteObject(bitmap);
            DeleteDC(dc);
        }
        std::cout << "LedRateStripTests passed\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
