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

TEST_CASE("Taskbar memory samples update both rows beside the separator", "[integration][rendering]")
{
    RateStrip strip;
    strip.SetContentVisible(true, true);
    const SIZE size = RateStrip::SizeForDpi(96, true);
    const int separator = RateStrip::SizeForDpi(96).cx + RateStrip::GroupGap / 2;
    test::DibSurface surface(size);
    REQUIRE(strip.RenderToDc(surface.dc, size, false));
    const std::vector<DWORD> unavailable(surface.pixels, surface.pixels + size.cx * size.cy);

    MEMORYSTATUSEX memory{sizeof(MEMORYSTATUSEX)};
    memory.dwMemoryLoad = 50;
    memory.ullTotalPhys = 32ull << 30;
    memory.ullAvailPhys = 16ull << 30;
    strip.SetMemorySample(memory);
    REQUIRE(strip.RenderToDc(surface.dc, size, false));
    CHECK(surface.pixels[(size.cy / 2) * size.cx + separator] != 0);
    for (int half : {0, 1})
    {
        bool changed = false;
        for (int y = half * size.cy / 2; y < (half + 1) * size.cy / 2; ++y)
        {
            const int row = y * size.cx;
            CHECK(std::equal(unavailable.begin() + row, unavailable.begin() + row + separator,
                surface.pixels + row));
            changed |= !std::equal(unavailable.begin() + row + separator + 1,
                unavailable.begin() + row + size.cx, surface.pixels + row + separator + 1);
        }
        CHECK(changed);
    }
    strip.SetMemorySample({});
    REQUIRE(strip.RenderToDc(surface.dc, size, false));
    CHECK(std::equal(unavailable.begin(), unavailable.end(), surface.pixels));
}

TEST_CASE("Memory icons follow the taskbar side and stay fixed as readings change", "[integration][rendering]")
{
    RateStrip strip;
    strip.SetContentVisible(true, true);
    for (bool left : {false, true})
    {
        REQUIRE(strip.SetTaskbarDisplayOnLeft(left));
        for (UINT dpi : {96u, 144u, 192u})
        {
            CAPTURE(left, dpi);
            const SIZE size = RateStrip::SizeForDpi(dpi, true);
            test::DibSurface surface(size);
            strip.SetMemorySample({});
            REQUIRE(strip.RenderToDc(surface.dc, size, false));
            const std::vector<DWORD> reference(surface.pixels, surface.pixels + size.cx * size.cy);
            MEMORYSTATUSEX sample{sizeof(MEMORYSTATUSEX)};
            sample.ullTotalPhys = 1024ull << 30;
            sample.dwMemoryLoad = 100;
            strip.SetMemorySample(sample);
            REQUIRE(strip.RenderToDc(surface.dc, size, false));
            const int first = left ? MulDiv(99 + RateStrip::GroupGap, dpi, 96) : size.cx - MulDiv(12, dpi, 96);
            const int last = left ? MulDiv(111 + RateStrip::GroupGap, dpi, 96) : size.cx - MulDiv(1, dpi, 96);
            for (int half : {0, 1})
            {
                bool lit = false;
                for (int y = half * size.cy / 2; y < (half + 1) * size.cy / 2; ++y)
                {
                    for (int x = first; x < last; ++x)
                    {
                        const int i = y * size.cx + x;
                        REQUIRE(surface.pixels[i] == reference[i]);
                        lit |= surface.pixels[i] != 0;
                    }
                }
                CHECK(lit);
            }
        }
    }
}

TEST_CASE("Memory percentage and capacity units share their left edge", "[integration][rendering]")
{
    RateStrip strip;
    strip.SetContentVisible(true, true);
    MEMORYSTATUSEX sample{sizeof(MEMORYSTATUSEX)};
    sample.ullTotalPhys = 500ull << 30;
    sample.ullAvailPhys = 300ull << 30;
    strip.SetMemorySample(sample);
    for (bool left : {false, true})
    {
        REQUIRE(strip.SetTaskbarDisplayOnLeft(left));
        for (UINT dpi : {96u, 144u, 192u})
        {
            CAPTURE(left, dpi);
            const SIZE size = RateStrip::SizeForDpi(dpi, true);
            test::DibSurface surface(size);
            REQUIRE(strip.RenderToDc(surface.dc, size, false));
            const auto firstUnitPixel = [&](int half)
            {
                for (int x = MulDiv(110, dpi, 96); x < size.cx; ++x)
                {
                    for (int y = half * size.cy / 2; y < (half + 1) * size.cy / 2; ++y)
                    {
                        const DWORD pixel = surface.pixels[y * size.cx + x];
                        const int alpha = pixel >> 24;
                        const int red = (pixel >> 16) & 255;
                        const int green = (pixel >> 8) & 255;
                        if (alpha > 32 && green > red && green - red < alpha / 12)
                            return x;
                    }
                }
                return static_cast<int>(size.cx);
            };
            const int percent = firstUnitPixel(0);
            const int gb = firstUnitPixel(1);
            REQUIRE(percent < size.cx);
            REQUIRE(gb < size.cx);
            CHECK(std::abs(percent - gb) <= 1);
        }
    }
}

TEST_CASE("Memory alone uses its own width and both groups leave space around the divider", "[integration][rendering]")
{
    RateStrip strip;
    const auto both = RateStrip::SizeForDpi(96, true);
    const auto rates = RateStrip::SizeForDpi(96);
    const auto memory = RateStrip::SizeForDpi(96, true, 3, false);
    const int offset = rates.cx + RateStrip::GroupGap;
    REQUIRE(both.cx == offset + memory.cx);
    for (bool left : {false, true})
    {
        REQUIRE(strip.SetTaskbarDisplayOnLeft(left));
        strip.SetContentVisible(true, true);
        test::DibSurface combined(both);
        REQUIRE(strip.RenderToDc(combined.dc, both, false));
        strip.SetContentVisible(false, true);
        strip.SetRates(L"invalid", L"invalid");
        test::DibSurface single(memory);
        REQUIRE(strip.RenderToDc(single.dc, memory, false));
        for (int y = 0; y < both.cy; ++y)
        {
            CHECK(combined.pixels[y * both.cx + rates.cx + 2] == 0);
            CHECK(combined.pixels[y * both.cx + offset - 2] == 0);
        }
        const auto ink = [](DWORD pixel) { return pixel != 0; };
        CHECK(std::count_if(single.pixels, single.pixels + memory.cx * memory.cy, ink) > 0);
        strip.SetRates(L"0.0 K/s", L"0.0 K/s");
    }
}

TEST_CASE("GPU-only strip updates both rows without needing a network reading", "[integration][rendering]")
{
    RateStrip strip;
    strip.SetContentVisible(false, false, true);
    strip.SetRates(L"invalid", L"invalid");
    const SIZE size = RateStrip::SizeForDpi(96, false, 3, false, true);
    REQUIRE(size.cx == RateStrip::GpuWidth);
    for (bool left : {true, false})
    {
        REQUIRE(strip.SetTaskbarDisplayOnLeft(left));
        test::DibSurface surface(size);
        strip.SetGpuSample({});
        REQUIRE(strip.RenderToDc(surface.dc, size, false));
        const std::vector<DWORD> missing(surface.pixels, surface.pixels + size.cx * size.cy);
        strip.SetGpuSample(GpuSample{68, 64});
        REQUIRE(strip.RenderToDc(surface.dc, size, false));
        for (int half : {0, 1})
        {
            const int start = half * size.cx * size.cy / 2;
            CHECK_FALSE(std::equal(missing.begin() + start, missing.begin() + start + size.cx * size.cy / 2,
                surface.pixels + start));
        }
    }
}

TEST_CASE("NVIDIA probe uses actual readings to determine availability", "[integration][gpu]")
{
    NvidiaGpuReader reader;
    const auto sample = reader.Probe();
    CHECK(reader.Available() == sample.has_value());
    if (sample)
    {
        CAPTURE(sample->load, sample->celsius);
        CHECK(sample->load <= 100);
    }
    else
    {
        CHECK_FALSE(reader.Read().has_value());
    }
}
