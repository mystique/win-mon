#include "FloatingRateRenderer.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <limits>
#include <filesystem>

void Require(bool value, const char* message)
{
    if (!value) { std::cerr << message << '\n'; std::exit(1); }
}
void SaveBitmap(const FloatingRateRenderer& renderer, const std::filesystem::path& path, DWORD background)
{
    SIZE size = renderer.PixelSize();
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + size.cx * size.cy * 4;
    BITMAPINFOHEADER info{sizeof(info), size.cx, -size.cy, 1, 32, BI_RGB};
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&info), sizeof(info));
    for (DWORD p : renderer.Pixels())
    {
        const DWORD inverse = 255 - (p >> 24);
        DWORD result = 0xff000000;
        for (int shift : {0, 8, 16})
            result |= (((p >> shift) & 255) + ((background >> shift) & 255) * inverse / 255) << shift;
        output.write(reinterpret_cast<const char*>(&result), sizeof(result));
    }
    Require(output.good(), "bitmap evidence written");
}
int main(int argc, char** argv)
{
    FloatingRateRenderer renderer;
    LOGFONTW font{};
    wcscpy_s(font.lfFaceName, L"Segoe UI");
    Require(renderer.Render(L"0.0 K/s", L"0.0 K/s", font, 96), "offscreen render");
    Require(renderer.PixelSize().cx == 138 && renderer.PixelSize().cy == 54, "132x48 body plus shadow");
    bool partial = false;
    const auto& pixels = renderer.Pixels();
    for (size_t i = 0; i < pixels.size(); ++i)
    {
        const DWORD p = pixels[i], a = p >> 24;
        Require((p & 255) <= a && ((p >> 8) & 255) <= a && ((p >> 16) & 255) <= a, "premultiplied alpha");
        if (i < 138 || i >= pixels.size() - 138 || i % 138 == 0 || i % 138 == 137)
            Require(p == 0, "transparent canvas boundary");
        partial |= a > 0 && a < 255;
    }
    Require(partial, "antialiased silhouette");
    Require(FloatingRateRenderer::IsPositionVisible({0,0,132,44}, {0,0,1920,1080}), "small saved body is visible");

    const auto colored = [&](int x, int y, bool up)
    {
        x = 3 + (x - 3) * 12 / 11;
        y = 3 + (y - 3) * 12 / 11;
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                DWORD p = renderer.Pixels()[(y + dy) * 138 + x + dx];
                int b = p & 255, g = (p >> 8) & 255, r = (p >> 16) & 255;
                if (up ? (g > 120 && g > b + 15 && g > r + 30) : (b > 140 && b > g + 15 && b > r + 50)) return true;
            }
        return false;
    };
    const auto render = [&] { Require(renderer.Render(L"0.5 K/s", L"0.5 K/s", font, 96), "sample render"); };
    renderer.AddSample(500, 0);
    render();
    Require(colored(37, 13, true) && colored(25, 8, true), "half upload reaches top counterclockwise");
    Require(!colored(13, 13, true) && !colored(37, 37, true), "upload stops at top and never enters bottom");
    renderer.ResetHistory();
    renderer.AddSample(0, 500);
    render();
    Require(colored(37, 37, false) && colored(25, 42, false), "half download reaches bottom clockwise");
    Require(!colored(13, 37, false) && !colored(37, 13, false), "download stops at bottom and never enters top");
    renderer.ResetHistory();
    renderer.AddSample(1000, 1000);
    render();
    Require(colored(13, 13, true) && colored(13, 37, false), "full semicircles reach left");
    for (UINT dpi : {96u, 120u, 144u, 192u})
    {
        for (bool active : {false, true})
        {
            renderer.ResetHistory();
            if (active) renderer.AddSample(1000, 1000);
            Require(renderer.Render(L"0.0 K/s", L"0.0 K/s", font, dpi), "joint gap frame");
            const int y = MulDiv(27, static_cast<int>(dpi), 96);
            for (int x : {MulDiv(8, static_cast<int>(dpi), 96), MulDiv(46, static_cast<int>(dpi), 96)})
                Require(renderer.Pixels()[y * renderer.PixelSize().cx + x] == 0xff222e34,
                    "tracks and full arcs leave capsule-colored gaps at both joints");
        }
    }
    renderer.AddSample(0, 0);
    render();
    Require(!colored(37, 13, true) && !colored(37, 37, false), "raw zero immediately clears arcs");

    renderer.ResetHistory();
    renderer.AddSample(1000, 500);
    render();
    Require(colored(13, 13, true) && !colored(13, 37, false), "independent ratios use common scale");
    renderer.AddSample(std::numeric_limits<double>::infinity(), std::numeric_limits<double>::quiet_NaN());
    render();
    Require(!colored(37, 13, true) && !colored(37, 37, false), "nonfinite raw input clears arcs");
    renderer.AddSample(-1, -1);
    render();
    Require(!colored(37, 13, true), "negative input clears arcs");
    renderer.ResetHistory();
    renderer.AddSample(1e9, 0);
    renderer.AddSample(1000, 0);
    render();
    Require(!colored(13, 13, true), "new lower rate retracts arc");
    for (int i = 0; i < 100; ++i) renderer.AddSample(1000, 0);
    render();
    Require(colored(13, 13, true), "expired peak no longer holds scale");
    renderer.ResetHistory();
    render();
    Require(!colored(37, 13, true), "network reset removes previous activity");
    const auto blank = renderer.Pixels();
    renderer.AddSample(0, 0);
    renderer.AddSample(0, 0);
    render();
    Require(renderer.Pixels() != blank, "actual zero history has a quiet baseline");
    const auto successful = renderer.Pixels();
    Require(!renderer.Render(L"0.0 K/s", L"0.0 K/s", font, 0), "invalid render rejected");
    Require(renderer.Pixels() == successful, "failure preserves successful bitmap");
    Require(!renderer.Present(nullptr), "failed submission contained");
    renderer.ReleaseTarget();
    render();
    Require(renderer.Pixels() == successful, "target rebuild preserves sample state");
    Require(!FloatingRateRenderer::HitTest({1, 25}, 96), "shadow passes input through");
    Require(FloatingRateRenderer::HitTest({25, 25}, 96), "capsule receives input");
    Require(!FloatingRateRenderer::IsPositionVisible({2000,0,2132,44}, {0,0,1920,1080}), "missing monitor rejected");
    Require(FloatingRateRenderer::IsPositionVisible({-84,0,48,44}, {0,0,1920,1080}), "usable partial body retained");
    renderer.ResetHistory();
    renderer.AddSample(500, 500);
    Require(renderer.Render(L"1.0 K/s", L"1.0 K/s", font, 96, 0.1), "early water wave");
    const auto earlyWave = renderer.Pixels();
    Require(renderer.Render(L"1.0 K/s", L"1.0 K/s", font, 96, 0.8), "late water wave");
    int waveChanges = 0;
    for (int y = 0; y < 54; ++y)
        for (int x = 0; x < 138; ++x)
            if (earlyWave[y * 138 + x] != renderer.Pixels()[y * 138 + x])
            {
                ++waveChanges;
                Require((x - 27) * (x - 27) + (y - 27) * (y - 27) < 17 * 17,
                    "water wave stays inside the inner circle");
            }
    Require(waveChanges > 15, "water surface moves between animation phases");
    int previousArea = -1;
    for (float level : {0.0f, 0.25f, 0.5f, 0.75f, 1.0f})
    {
        Require(renderer.Render(L"", L"", font, 96, 0.5, level), "water level frame");
        int area = 0;
        for (int y = 11; y <= 42; ++y)
            for (int x = 11; x <= 42; ++x)
                if ((x - 27) * (x - 27) + (y - 27) * (y - 27) < 15 * 15)
                    area += renderer.Pixels()[y * 138 + x] != 0xff222e34;
        Require(area > previousArea, "water fills progressively from empty to full");
        previousArea = area;
    }
    Require(renderer.Render(L"", L"", font, 96, 0.5, 0.25f), "falling water frame");
    const auto lowWater = renderer.Pixels();
    Require(lowWater[18 * 138 + 27] == 0xff222e34 && lowWater[37 * 138 + 27] != 0xff222e34,
        "receding water clears the top and retains the bottom");
    renderer.AddSample(0, 0);
    Require(renderer.Render(L"0.0 K/s", L"0.0 K/s", font, 96, 0.1), "idle water frame");
    const auto idle = renderer.Pixels();
    Require(renderer.Render(L"0.0 K/s", L"0.0 K/s", font, 96, 0.8), "idle second phase");
    Require(idle == renderer.Pixels(), "idle water is empty and still");
    renderer.ResetHistory();
    render();
    const auto emptyCurves = renderer.Pixels();
    for (int i = 0; i < 48; ++i) renderer.AddSample(1000, 500);
    render();
    bool greenCurve = false, blueCurve = false;
    for (int y = 8; y < 28; ++y)
        for (int x = 52; x < 123; ++x)
        {
            const DWORD p = renderer.Pixels()[y * 138 + x];
            if (p == emptyCurves[y * 138 + x]) continue;
            const int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            greenCurve |= g > r + 15 && g > b + 5;
            blueCurve |= b > r + 15 && b > g + 8;
        }
    Require(greenCurve && blueCurve, "both color-coded curves use the upper area too");
    const auto curves = renderer.Pixels();
    Require(renderer.Render(L"999.9 G/s", L"123.4 M/s", font, 96), "changed rate text");
    Require(curves == renderer.Pixels(), "upload and download text are both absent");
    const DWORD upperFill = renderer.Pixels()[32 * 138 + 80];
    const DWORD lowerFill = renderer.Pixels()[41 * 138 + 80];
    Require(upperFill != 0xff222e34 && lowerFill != 0xff222e34 && upperFill != lowerFill,
        "area below curves has a fading gradient");
    renderer.ResetHistory();
    for (int i = 0; i < 46; ++i) renderer.AddSample(0, 0);
    renderer.AddSample(0, 1000);
    renderer.AddSample(0, 0);
    Require(renderer.Render(L"0.0 K/s", L"0.0 K/s", font, 96, 0.5, 0), "isolated trend peak");
    bool retainedPeak = false;
    for (int y = 5; y < 50; ++y)
        for (int x = 115; x < 126; ++x)
        {
            const DWORD p = renderer.Pixels()[y * 138 + x];
            const int r = (p >> 16) & 255, g = (p >> 8) & 255, b = p & 255;
            if (b > r + 15 && b > g + 8)
            {
                Require(y >= 30 && y <= 45, "smooth trend stays inside sampled range plus stroke coverage");
                retainedPeak |= x >= 119 && y <= 33;
            }
        }
    Require(retainedPeak, "smoothing preserves the isolated sampled peak");
    const auto evidence = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path{};
    if (!evidence.empty()) std::filesystem::create_directories(evidence);
    for (UINT dpi : {96u, 120u, 144u, 192u})
        for (const wchar_t* family : {L"Segoe UI", L"Arial Black"})
            for (const wchar_t* value : {L"0.0 K/s", L"999.9 K/s", L"999.9 G/s", L"123456789.0 G/s"})
            {
                wcscpy_s(font.lfFaceName, family);
                Require(renderer.Render(value, value, font, dpi), "DPI/font/long value matrix");
                const SIZE size = renderer.PixelSize();
                Require(size.cx == (dpi == 96 ? 138 : dpi == 120 ? 173 : dpi == 144 ? 207 : 276), "scaled width");
                Require(size.cy == (dpi == 96 ? 54 : dpi == 120 ? 68 : dpi == 144 ? 81 : 108), "scaled height");
                for (size_t i = 0; i < renderer.Pixels().size(); ++i)
                {
                    DWORD p = renderer.Pixels()[i], a = p >> 24;
                    Require((p & 255) <= a && ((p >> 8) & 255) <= a && ((p >> 16) & 255) <= a, "matrix premultiplication");
                    if (i < static_cast<size_t>(size.cx) || i >= renderer.Pixels().size() - size.cx || i % size.cx == 0 || i % size.cx == size.cx - 1)
                        Require(p == 0, "matrix no content escapes canvas");
                }
            }
    wcscpy_s(font.lfFaceName, L"Segoe UI");
    // A reused renderer must match a fresh one after text, font, DPI and history changes.
    FloatingRateRenderer cached;
    for (int sample = 0; sample < 4; ++sample)
    {
        cached.AddSample(sample * 1200, (4 - sample) * 9000);
        for (UINT dpi : {96u, 192u, 96u})
            for (BYTE decoration : {BYTE{0}, BYTE{1}})
                for (float hover : {0.0f, 0.4f, 1.0f, 0.0f})
                {
                    LOGFONTW selected = font;
                    selected.lfUnderline = decoration;
                    selected.lfItalic = decoration;
                    const auto value = sample % 2 ? L"999.9 G/s" : L"0.1 K/s";
                    FloatingRateRenderer fresh;
                    for (int i = 0; i <= sample; ++i) fresh.AddSample(i * 1200, (4 - i) * 9000);
                    Require(cached.Render(value, value, selected, dpi, 0.3, hover), "cached frame");
                    Require(fresh.Render(value, value, selected, dpi, 0.3, hover), "fresh frame");
                    Require(cached.Pixels() == fresh.Pixels(), "cache invalidation matches fresh renderer");
                    const auto expected = cached.Pixels();
                    Require(cached.Render(value, value, selected, dpi, 0.3, hover), "repeat cached frame");
                    Require(cached.Pixels() == expected, "cached frame remains identical");
                }
    }
    cached.ResetHistory();
    cached.ReleaseTarget();
    FloatingRateRenderer empty;
    Require(cached.Render(L"0.0 K/s", L"0.0 K/s", font, 96), "recreate cached target");
    Require(empty.Render(L"0.0 K/s", L"0.0 K/s", font, 96), "empty reference");
    Require(cached.Pixels() == empty.Pixels(), "reset clears cached sample geometry");
    renderer.ResetHistory();
    for (int i = 0; i < 48; ++i) renderer.AddSample(128000, 400000 + (i % 7) * 120000);
    Require(renderer.Render(L"128.0 K/s", L"8.4 M/s", font, 96), "evidence frame");
    if (!evidence.empty())
    {
        SaveBitmap(renderer, evidence / "actual-size-dark.bmp", 0x101419);
        SaveBitmap(renderer, evidence / "actual-size-light.bmp", 0xf4f5f7);
        Require(renderer.Render(L"999.9 G/s", L"999.9 G/s", font, 96), "long evidence");
        SaveBitmap(renderer, evidence / "long-reading.bmp", 0xf4f5f7);
        Require(renderer.Render(L"0.6 K/s", L"0.1 K/s", font, 96), "short evidence");
        SaveBitmap(renderer, evidence / "short-reading.bmp", 0x181818);
        for (int frame = 0; frame < 30; ++frame)
        {
            Require(renderer.Render(L"128.0 K/s", L"8.4 M/s", font, 96, frame / 30.0, frame < 15 ? frame / 14.0f : (29 - frame) / 14.0f), "hover transition preview frame");
            SaveBitmap(renderer, evidence / ("pulse-" + std::to_string(frame) + ".bmp"), 0x181818);
        }
    }
}
