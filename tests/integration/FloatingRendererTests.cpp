#include "floating/FloatingRateRenderer.h"
#include <catch2/catch_test_macros.hpp>
#include <limits>

namespace
{
struct RenderFixture
{
    FloatingRateRenderer renderer;
    void render()
    {
        REQUIRE(renderer.Render(L"0.0 K/s", L"0.0 K/s", 96, 0.3, 0));
    }
    static int X(float x)
    {
        return static_cast<int>(27 + (x - 20) * (37.0f * 12 / 11 / 24));
    }
    static int Y(float y)
    {
        return static_cast<int>(27 + (y - 27) * (37.0f * 12 / 11 / 24));
    }
    bool colored(int x, int y, bool up)
    {
        x = X(static_cast<float>(x));
        y = Y(static_cast<float>(y));
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const DWORD pixel = renderer.Pixels()[(y + dy) * renderer.PixelSize().cx + x + dx];
                const int green = (pixel >> 8) & 255, blue = pixel & 255;
                if (up ? green > 120 && green > blue + 15 : blue > 140 && blue > green + 15)
                    return true;
            }
        return false;
    }
};
} // namespace

TEST_CASE("Scrolling trends stay clipped between samples", "[integration][rendering]")
{

    FloatingRateRenderer scrolling;
    REQUIRE(!scrolling.HasScrollingTrend());
    for (int i = 0; i < 40; ++i)
        scrolling.AddSample(i % 4 * 10000, (i + 2) % 5 * 10000);
    REQUIRE(scrolling.HasScrollingTrend());
    const auto scrollFrame = [&](float progress) {
        REQUIRE(scrolling.Render(L"12.3 K/s", L"45.6 K/s", 384, 0.3, 0, 1, nullptr, 1, progress));
        return scrolling.Pixels();
    };
    const auto startScroll = scrollFrame(0);
    const auto middleScroll = scrollFrame(0.5f);
    const auto endScroll = scrollFrame(1);
    REQUIRE((startScroll != middleScroll && middleScroll != endScroll));
    REQUIRE(scrollFrame(1) == endScroll);
    for (int y = 0; y < scrolling.PixelSize().cy; ++y)
        for (int x = 0; x < scrolling.PixelSize().cx; ++x)
            if (startScroll[y * scrolling.PixelSize().cx + x] != endScroll[y * scrolling.PixelSize().cx + x])
                REQUIRE((x >= 208 && x <= 472 && y >= 24 && y <= 192));
    REQUIRE(!scrolling.Render(L"12.3 K/s", L"45.6 K/s", 384, 0.3, 0, 1, nullptr, 1, -1));

    scrolling.ResetHistory();
    scrolling.AddSample(0, 0);
    scrolling.AddSample(0, 0);
    REQUIRE(!scrolling.HasScrollingTrend());
}

TEST_CASE_METHOD(RenderFixture, "Ring arcs and trends retain their geometry", "[integration][rendering]")
{
    for (bool up : {true, false})
        for (double ratio : {0.0, 0.25, 0.5, 1.0})
        {
            renderer.ResetHistory();
            renderer.AddSample(up ? ratio * 1000 : 0, up ? 0 : ratio * 1000);
            render();
            const int y = up ? 23 : 31;
            REQUIRE((colored(10, y, up) == (ratio > 0) && colored(30, y, up) == (ratio > 0)));
            REQUIRE(colored(20, up ? 16 : 38, up) == (ratio == 1));
            REQUIRE((renderer.Pixels()[27 * renderer.PixelSize().cx + X(8)] == 0xff222e34 &&
                     renderer.Pixels()[27 * renderer.PixelSize().cx + X(32)] == 0xff222e34));
        }
    renderer.AddSample(0, 0);
    render();
    REQUIRE(!colored(10, 31, false));
    renderer.AddSample(-1, std::numeric_limits<double>::quiet_NaN());
    render();
    REQUIRE(renderer.Activity() == 0);
    renderer.ResetHistory();
    render();
    const auto empty = renderer.Pixels();
    for (int i = 0; i < 32; ++i)
        renderer.AddSample(1000, 500);
    render();
    for (int y = 0; y < 54; ++y)
        for (int x = X(33); x < renderer.PixelSize().cx; ++x)
            if (renderer.Pixels()[y * renderer.PixelSize().cx + x] != empty[y * renderer.PixelSize().cx + x])
                REQUIRE((x >= X(35) && x <= X(74) && y >= Y(15) && y <= Y(39)));
    REQUIRE((colored(50, 15, true) && colored(50, 33, false)));
    const auto beforeText = renderer.Pixels();
    REQUIRE(renderer.Render(L"999.9 G/s", L"12.8 M/s", 96, 0.3, 0));
    REQUIRE(renderer.Pixels() != beforeText);
    const auto successful = renderer.Pixels();
    REQUIRE(!renderer.Render(L"0.0 K/s", L"0.0 K/s", 0));
    REQUIRE(renderer.Pixels() == successful);
    renderer.ReleaseTarget();
    REQUIRE(renderer.Render(L"999.9 G/s", L"12.8 M/s", 96, 0.3, 0));
    REQUIRE(renderer.Pixels() == successful);
    REQUIRE((FloatingRateRenderer::HitTest({25, 27}, 96) && !FloatingRateRenderer::HitTest({1, 27}, 96)));
    REQUIRE(FloatingRateRenderer::IsPositionVisible({0, 0, 132, 48}, {0, 0, 1920, 1080}));
}

TEST_CASE_METHOD(RenderFixture, "Water animation affects only the ring", "[integration][rendering]")
{
    renderer.ResetHistory();
    renderer.AddSample(1000, 1000);
    REQUIRE(renderer.Render(L"1.0 K/s", L"1.0 K/s", 96, 0.1));
    const auto water = renderer.Pixels();
    REQUIRE(renderer.Render(L"1.0 K/s", L"1.0 K/s", 96, 0.7));
    int changed = 0;
    for (int y = 0; y < 54; ++y)
        for (int x = 0; x < renderer.PixelSize().cx; ++x)
            if (renderer.Pixels()[y * renderer.PixelSize().cx + x] != water[y * renderer.PixelSize().cx + x])
            {
                ++changed;
                REQUIRE((x - 27) * (x - 27) + (y - 27) * (y - 27) < 290);
            }
    REQUIRE(changed > 8);
}

TEST_CASE_METHOD(RenderFixture, "Text visibility retains charts at every DPI", "[integration][rendering]")
{
    for (UINT dpi : {96u, 120u, 144u, 192u})
    {
        REQUIRE(renderer.Render(L"999.9 G/s", L"12.8 M/s", dpi, 0.3, 0.5f, 1));
        const auto expanded = renderer.Pixels();
        const auto expandedSize = renderer.PixelSize();
        REQUIRE(!renderer.Render(L"invalid", L"12.8 M/s", dpi));
        REQUIRE(expanded == renderer.Pixels());
        for (float progress : {0.5f, 0.0f, 1.0f})
        {
            REQUIRE(renderer.Render(L"999.9 G/s", L"12.8 M/s", dpi, 0.3, 0.5f, progress));
            const auto size = renderer.PixelSize();
            REQUIRE((size.cy == expandedSize.cy && (progress == 1 || size.cx < expandedSize.cx)));
            for (int y = MulDiv(Y(15) + 1, dpi, 96); y < MulDiv(Y(39), dpi, 96); ++y)
                for (int x = MulDiv(X(8), dpi, 96); x < MulDiv(X(74), dpi, 96); ++x)
                    REQUIRE(renderer.Pixels()[y * size.cx + x] == expanded[y * expandedSize.cx + x]);
        }
        REQUIRE(renderer.Render(L"1000.0 K/s", L"1000.0 M/s", dpi));
        const auto saturated = renderer.Pixels();
        REQUIRE(renderer.Render(L"999.9 K/s", L"999.9 M/s", dpi));
        REQUIRE(saturated == renderer.Pixels());
    }
}

TEST_CASE_METHOD(RenderFixture, "Trend history retains exactly sixteen visible samples", "[integration][rendering]")
{
    renderer.ResetHistory();
    renderer.AddSample(1000, 0);
    for (int i = 0; i < 15; ++i)
        renderer.AddSample(0, 0);
    render();
    REQUIRE(colored(35, 15, true));
    renderer.AddSample(0, 0);
    render();
    REQUIRE(!colored(35, 15, true));
}

TEST_CASE_METHOD(RenderFixture, "Embedded LED lamps and readout transitions are stable", "[integration][rendering]")
{
    renderer.ResetHistory();
    REQUIRE(renderer.Render(L"0.0 K/s", L"0.0 K/s", 384, 0.3, 0));
    const char* arrow[] = {"..#..", ".###.", "#.#.#", "..#..", "..#..", "..#..", "..#.."};
    for (int row = 0; row < 7; ++row)
        for (int column = 0; column < 5; ++column)
        {
            const int x = static_cast<int>((27 + (77 + column * 1.05f - 20) * (37.0f * 12 / 11 / 24)) * 4);
            const int y = static_cast<int>((27 + (23.8f - (6 - row) * 1.05f - 27) * (37.0f * 12 / 11 / 24)) * 4);
            const DWORD pixel = renderer.Pixels()[y * renderer.PixelSize().cx + x];
            const int green = (pixel >> 8) & 255;
            REQUIRE((green > 100) == (arrow[row][column] == '#'));
            REQUIRE(pixel != 0xff222e34);
            const int leadingX = static_cast<int>((27 + (83.3f + column * 1.05f - 20) * (37.0f * 12 / 11 / 24)) * 4);
            const DWORD reserved = renderer.Pixels()[y * renderer.PixelSize().cx + leadingX];
            REQUIRE((reserved != 0xff222e34 && ((reserved >> 8) & 255) < 100));
        }
    for (int row = 0; row < 7; ++row)
        for (int column = 0; column < 3; ++column)
        {
            const int x = static_cast<int>((27 + (100.625f + column * 1.05f - 20) * (37.0f * 12 / 11 / 24)) * 4);
            const int y = static_cast<int>((27 + (23.8f - (6 - row) * 1.05f - 27) * (37.0f * 12 / 11 / 24)) * 4);
            const DWORD pixel = renderer.Pixels()[y * renderer.PixelSize().cx + x];
            REQUIRE(pixel != 0xff222e34);
            REQUIRE((((pixel >> 8) & 255) > 100) == (row == 6 && column == 1));
        }
    const int brightX = static_cast<int>((27 + (95.9f - 20) * (37.0f * 12 / 11 / 24)) * 4);
    const int brightY = static_cast<int>((27 + (17.5f - 27) * (37.0f * 12 / 11 / 24)) * 4);
    REQUIRE(((renderer.Pixels()[brightY * renderer.PixelSize().cx + brightX] >> 8) & 255) > 245);
    for (wchar_t digit = L'0'; digit <= L'9'; ++digit)
        for (wchar_t unit : {L'K', L'M', L'G'})
        {
            std::wstring value{digit, digit, digit, L'.', digit, L' ', unit, L'/', L's'};
            REQUIRE(renderer.Render(value, value, 96));
        }
    renderer.ResetHistory();
    LedReadoutTransition change{L"128.0 K/s", L"128.0 K/s", 0};
    REQUIRE(renderer.Render(change.upload, change.download, 384, 0.3, 0));
    const auto oldReading = renderer.Pixels();
    REQUIRE(renderer.Render(L"129.0 K/s", L"129.0 K/s", 384, 0.3, 0, 1, &change));
    REQUIRE(renderer.Pixels() == oldReading);
    change.progress = 0.5f;
    REQUIRE(renderer.Render(L"129.0 K/s", L"129.0 K/s", 384, 0.3, 0, 1, &change));
    REQUIRE(((renderer.Pixels()[brightY * renderer.PixelSize().cx + brightX] >> 8) & 255) < 100);
    for (int y = 0; y < renderer.PixelSize().cy; ++y)
        for (int x = 0; x < static_cast<int>((27 + (94.3f - 20) * (37.0f * 12 / 11 / 24)) * 4); ++x)
            REQUIRE(renderer.Pixels()[y * renderer.PixelSize().cx + x] == oldReading[y * renderer.PixelSize().cx + x]);
    change.progress = 1;
    REQUIRE(renderer.Render(L"129.0 K/s", L"129.0 K/s", 384, 0.3, 0, 1, &change));
    const auto settled = renderer.Pixels();
    REQUIRE(renderer.Render(L"129.0 K/s", L"129.0 K/s", 384, 0.3, 0));
    REQUIRE(renderer.Pixels() == settled);
}

TEST_CASE_METHOD(RenderFixture, "Independent visibility and alpha work at every DPI", "[integration][rendering]")
{
    REQUIRE(renderer.Render(L"12.3 K/s", L"45.6 M/s", 96, 0.3, 0.5, 0, nullptr, 0));
    REQUIRE((renderer.PixelSize().cx == 54 && renderer.PixelSize().cy == 54));
    REQUIRE((!FloatingRateRenderer::HitTest({50, 5}, 96, 0, 0) && FloatingRateRenderer::HitTest({27, 27}, 96, 0, 0)));
    const auto circlePixels = renderer.Pixels();
    REQUIRE(renderer.Render(L"999.9 G/s", L"999.9 M/s", 96, 0.3, 0.5, 1, nullptr, 0));
    for (int y = 7; y <= 47; ++y)
        for (int x = 7; x <= 47; ++x)
            if ((x - 27) * (x - 27) + (y - 27) * (y - 27) <= 420)
                REQUIRE(renderer.Pixels()[y * renderer.PixelSize().cx + x] == circlePixels[y * 54 + x]);
    REQUIRE(renderer.PixelSize().cx >= 149);
    for (float charts : {0.0f, 0.5f, 1.0f})
        for (float text : {0.0f, 0.5f, 1.0f})
            REQUIRE(renderer.Render(L"12.3 K/s", L"45.6 M/s", 144, 0.3, 0.5, text, nullptr, charts));
    renderer.ResetHistory();
    for (int i = 0; i < 32; ++i)
        renderer.AddSample(300000 + (i % 5) * 70000, 700000 + (i % 7) * 140000);
    for (UINT dpi : {96u, 120u, 144u, 192u})
    {
        REQUIRE(renderer.Render(L"999.9 G/s", L"999.9 M/s", dpi));
        for (DWORD pixel : renderer.Pixels())
        {
            const DWORD alpha = pixel >> 24;
            REQUIRE(((pixel & 255) <= alpha && ((pixel >> 8) & 255) <= alpha && ((pixel >> 16) & 255) <= alpha));
        }
    }
}
