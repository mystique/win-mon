#include "taskbar/RateStrip.h"
#include "support/WindowsFixtures.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>
#include <cmath>
#include <vector>

TEST_CASE("Taskbar lamps retain palette spacing and alpha at every DPI", "[integration][rendering]")
{
    RateStrip strip;
    for (UINT dpi : {96u, 120u, 144u, 192u})
        for (bool left : {false, true})
        {
            REQUIRE(strip.SetTaskbarDisplayOnLeft(left));
            CAPTURE(dpi, left);
            const SIZE size = RateStrip::SizeForDpi(dpi);
            test::DibSurface surface(size);
            HDC dc = surface.dc;
            auto* pixels = surface.pixels;
            std::vector<DWORD> dark;
            for (bool light : {false, true})
            {
                strip.SetRates(L"999.9 G/s", L"12.8 M/s");
                REQUIRE(strip.RenderToDc(dc, size, light));
                const size_t count = static_cast<size_t>(size.cx) * size.cy;
                REQUIRE(std::count_if(pixels, pixels + count, [](DWORD p) { return (p >> 24) != 0; }) >
                        static_cast<std::ptrdiff_t>(count / 10));
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
                            if ((p >> 24) > 160 && (upload ? g > b + 15 : b > g + 15))
                                coloredNumber = true;
                        }
                    REQUIRE(coloredNumber);
                }
                for (int x = 0; x < size.cx; ++x)
                    REQUIRE((pixels[x] == 0 && pixels[(size.cy - 1) * size.cx + x] == 0));
                for (int y = 0; y < size.cy; ++y)
                    REQUIRE((pixels[y * size.cx] == 0 && pixels[y * size.cx + size.cx - 1] == 0));
                for (size_t i = 0; i < count; ++i)
                    REQUIRE((((pixels[i] >> 16) & 255) <= (pixels[i] >> 24) &&
                             ((pixels[i] >> 8) & 255) <= (pixels[i] >> 24) && (pixels[i] & 255) <= (pixels[i] >> 24)));
                if (!light)
                    dark.assign(pixels, pixels + count);
                else
                    REQUIRE(!std::equal(dark.begin(), dark.end(), pixels));
            }
            REQUIRE(strip.RenderToDc(dc, size, false));
            REQUIRE(std::equal(dark.begin(), dark.end(), pixels));
            strip.SetRates(L"1.8 K/s", L"1.8 K/s");
            REQUIRE(strip.RenderToDc(dc, size, false));
            if (!left)
            {
                const int gapLeft = static_cast<int>(std::ceil(42.0f * size.cx / 55));
                const int gapRight = static_cast<int>(46.0f * size.cx / 55);
                bool nearRightEdge = false;
                for (int y = 0; y < size.cy; ++y)
                {
                    for (int x = gapLeft; x < gapRight; ++x)
                        REQUIRE(pixels[y * size.cx + x] == 0);
                    for (int x = size.cx - MulDiv(9, dpi, 96); x < size.cx; ++x)
                        nearRightEdge |= (pixels[y * size.cx + x] >> 24) != 0;
                }
                REQUIRE(nearRightEdge);
            }
            const float blankX = left ? 11.5f : 2.0f;
            const int blankLeft = static_cast<int>((blankX - 0.42f) * size.cx / 55);
            const int blankRight = static_cast<int>(std::ceil((blankX + 4.2f + 0.42f) * size.cx / 55));
            for (int y = 0; y < size.cy; ++y)
                for (int x = blankLeft; x < blankRight; ++x)
                    REQUIRE(pixels[y * size.cx + x] == 0);
            if (left)
            {
                strip.SetRates(L"888.8 K/s", L"888.8 K/s");
                REQUIRE(strip.RenderToDc(dc, size, false));
                const int gapLeft = static_cast<int>(std::ceil(7.0f * size.cx / 55));
                const int gapRight = static_cast<int>(11.0f * size.cx / 55);
                for (int y = 0; y < size.cy; ++y)
                    for (int x = gapLeft; x < gapRight; ++x)
                        REQUIRE(pixels[y * size.cx + x] == 0);
            }
        }
}
