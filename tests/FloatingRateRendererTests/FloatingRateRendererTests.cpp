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
    Require(FloatingRateRenderer::SizeForDpi(96).cx == 220 && FloatingRateRenderer::SizeForDpi(96).cy == 54,
        "original ring and readable LED cells widen the window without changing height");
    FloatingRateRenderer renderer;
    const auto render = [&] { Require(renderer.Render(L"0.0 K/s", L"0.0 K/s", 96, 0.3, 0), "render"); };
    const auto X = [](float x) { return static_cast<int>(27 + (x-20)*(37.0f*12/11/24)); };
    const auto Y = [](float y) { return static_cast<int>(27 + (y-27)*(37.0f*12/11/24)); };
    const auto colored = [&](int x, int y, bool up) {
        x = X(static_cast<float>(x)); y = Y(static_cast<float>(y));
        for (int dy = -1; dy <= 1; ++dy)
            for (int dx = -1; dx <= 1; ++dx)
            {
                const DWORD p = renderer.Pixels()[(y + dy) * renderer.PixelSize().cx + x + dx];
                const int g = (p >> 8) & 255, b = p & 255;
                if (up ? g > 120 && g > b + 15 : b > 140 && b > g + 15) return true;
            }
        return false;
    };
    for (bool up : {true, false})
        for (double ratio : {0.0, 0.25, 0.5, 1.0})
        {
            renderer.ResetHistory();
            renderer.AddSample(up ? ratio * 1000 : 0, up ? 0 : ratio * 1000);
            render();
            const int y = up ? 23 : 31;
            Require(colored(10, y, up) == (ratio > 0) && colored(30, y, up) == (ratio > 0), "both arc ends grow symmetrically");
            Require(colored(20, up ? 16 : 38, up) == (ratio == 1), "midpoint stays dark until full");
            Require(renderer.Pixels()[27 * renderer.PixelSize().cx + X(8)] == 0xff222e34 && renderer.Pixels()[27 * renderer.PixelSize().cx + X(32)] == 0xff222e34, "joint gaps");
        }
    renderer.AddSample(0, 0); render();
    Require(!colored(10, 31, false), "raw zero clears arc immediately");
    renderer.AddSample(-1, std::numeric_limits<double>::quiet_NaN()); render();
    Require(renderer.Activity() == 0, "invalid rates are idle");
    renderer.ResetHistory(); render();
    const auto empty = renderer.Pixels();
    for (int i = 0; i < 32; ++i) renderer.AddSample(1000, 500);
    render();
    for (int y = 0; y < 54; ++y)
        for (int x = X(33); x < renderer.PixelSize().cx; ++x)
            if (renderer.Pixels()[y * renderer.PixelSize().cx + x] != empty[y * renderer.PixelSize().cx + x])
                Require(x >= X(35) && x <= X(74) && y >= Y(15) && y <= Y(39), "curves and fill clipped to independent regions");
    Require(colored(50, 15, true) && colored(50, 33, false), "independent upload/download curves");
    const auto beforeText = renderer.Pixels();
    Require(renderer.Render(L"999.9 G/s", L"12.8 M/s", 96, 0.3, 0), "LED frame");
    Require(renderer.Pixels() != beforeText, "readouts are actually rendered");
    const auto successful = renderer.Pixels();
    Require(!renderer.Render(L"0.0 K/s", L"0.0 K/s", 0), "invalid DPI rejected");
    Require(renderer.Pixels() == successful, "invalid frame preserves success");
    renderer.ReleaseTarget(); Require(renderer.Render(L"999.9 G/s", L"12.8 M/s", 96, 0.3, 0), "rebuild");
    Require(renderer.Pixels() == successful, "target rebuild retains geometry");
    Require(FloatingRateRenderer::HitTest({25,27}, 96) && !FloatingRateRenderer::HitTest({1,27}, 96), "body/shadow hit testing");
    Require(FloatingRateRenderer::IsPositionVisible({0,0,132,48},{0,0,1920,1080}), "saved body visible");
    renderer.ResetHistory(); renderer.AddSample(1000,1000);
    Require(renderer.Render(L"1.0 K/s", L"1.0 K/s", 96, 0.1), "high activity first phase");
    const auto water = renderer.Pixels();
    Require(renderer.Render(L"1.0 K/s", L"1.0 K/s", 96, 0.7), "high activity next phase");
    int changed = 0;
    for (int y = 0; y < 54; ++y)
        for (int x = 0; x < renderer.PixelSize().cx; ++x)
            if (renderer.Pixels()[y * renderer.PixelSize().cx + x] != water[y * renderer.PixelSize().cx + x])
            {
                ++changed;
                Require((x-27)*(x-27)+(y-27)*(y-27) < 290, "water stays inside circle, charts and text remain stationary");
            }
    Require(changed > 8, "high activity retains moving waterline");
    for (UINT dpi : {96u,120u,144u,192u})
    {
        Require(renderer.Render(L"999.9 G/s", L"12.8 M/s", dpi, 0.3, 0.5f, 1), "expanded frame");
        const auto expanded = renderer.Pixels();
        const auto expandedSize = renderer.PixelSize();
        Require(!renderer.Render(L"invalid", L"12.8 M/s", dpi), "unknown glyph rejected");
        Require(expanded == renderer.Pixels(), "invalid glyph preserves frame");
        for (float progress : {0.5f,0.0f,1.0f})
        {
            Require(renderer.Render(L"999.9 G/s", L"12.8 M/s", dpi, 0.3, 0.5f, progress), "text visibility frame");
            const auto size = renderer.PixelSize();
            Require(size.cy == expandedSize.cy && (progress == 1 || size.cx < expandedSize.cx), "real canvas narrows without changing height");
            for (int y = MulDiv(Y(15)+1,dpi,96); y < MulDiv(Y(39),dpi,96); ++y)
                for (int x = MulDiv(X(8),dpi,96); x < MulDiv(X(74),dpi,96); ++x)
                    Require(renderer.Pixels()[y * size.cx + x] == expanded[y * expandedSize.cx + x], "rings and charts remain fixed through transition");
        }
        Require(renderer.Render(L"1000.0 K/s", L"1000.0 M/s", dpi), "rounding overflow");
        const auto saturated = renderer.Pixels();
        Require(renderer.Render(L"999.9 K/s", L"999.9 M/s", dpi), "capacity frame");
        Require(saturated == renderer.Pixels(), "overflow saturates full numeric field");
    }
    renderer.ResetHistory(); renderer.AddSample(1000,0);
    for(int i=0;i<31;++i) renderer.AddSample(0,0);
    render();
    Require(colored(35,15,true), "oldest real sample reaches left edge at 32 samples");
    renderer.AddSample(0,0); render();
    Require(!colored(35,15,true), "oldest sample exits on next measurement");
    renderer.ResetHistory();
    Require(renderer.Render(L"0.0 K/s",L"0.0 K/s",384,0.3,0), "lamp-cell inspection frame");
    const char* arrow[] = {"..#..", ".###.", "#.#.#", "..#..", "..#..", "..#..", "..#.."};
    for(int row=0;row<7;++row)
        for(int column=0;column<5;++column)
        {
            const int x=static_cast<int>((27+(77+column*1.05f-20)*(37.0f*12/11/24))*4);
            const int y=static_cast<int>((27+(23.8f-(6-row)*1.05f-27)*(37.0f*12/11/24))*4);
            const DWORD pixel=renderer.Pixels()[y*renderer.PixelSize().cx+x];
            const int green=(pixel>>8)&255;
            Require((green>100)==(arrow[row][column]=='#'), "each of 35 arrow lamps matches Punctum data");
            Require(pixel!=0xff222e34, "unlit lamps have their own dim background");
            const int leadingX=static_cast<int>((27+(83.3f+column*1.05f-20)*(37.0f*12/11/24))*4);
            const DWORD reserved=renderer.Pixels()[y*renderer.PixelSize().cx+leadingX];
            Require(reserved!=0xff222e34 && ((reserved>>8)&255)<100, "reserved integer cell has 35 dark lamps and no leading zero");
        }
    for (int row = 0; row < 7; ++row)
        for (int column = 0; column < 3; ++column)
        {
            const int x = static_cast<int>((27 + (100.625f + column * 1.05f - 20) * (37.0f * 12 / 11 / 24)) * 4);
            const int y = static_cast<int>((27 + (23.8f - (6 - row) * 1.05f - 27) * (37.0f * 12 / 11 / 24)) * 4);
            const DWORD pixel = renderer.Pixels()[y * renderer.PixelSize().cx + x];
            Require(pixel != 0xff222e34, "decimal owns 21 lamps in a 3x7 cell");
            Require((((pixel >> 8) & 255) > 100) == (row == 6 && column == 1), "decimal lights only its middle baseline lamp");
        }
    const int brightX = static_cast<int>((27 + (95.9f - 20) * (37.0f * 12 / 11 / 24)) * 4);
    const int brightY = static_cast<int>((27 + (17.5f - 27) * (37.0f * 12 / 11 / 24)) * 4);
    Require(((renderer.Pixels()[brightY * renderer.PixelSize().cx + brightX] >> 8) & 255) > 245,
        "numeric bright lamps have stronger luminance than the original green");
    for(wchar_t digit=L'0';digit<=L'9';++digit)
        for(wchar_t unit : {L'K',L'M',L'G'})
        {
            std::wstring value{digit,digit,digit,L'.',digit,L' ',unit,L'/',L's'};
            Require(renderer.Render(value,value,96), "all digit and unit glyphs render");
        }
    renderer.ResetHistory();
    LedReadoutTransition change{L"128.0 K/s", L"128.0 K/s", 0};
    Require(renderer.Render(change.upload,change.download,384,0.3,0), "old readout");
    const auto oldReading = renderer.Pixels();
    Require(renderer.Render(L"129.0 K/s",L"129.0 K/s",384,0.3,0,1,&change), "start LED transition");
    Require(renderer.Pixels() == oldReading, "LED transition starts at the old glyph without a switch");
    change.progress = 0.5f;
    Require(renderer.Render(L"129.0 K/s",L"129.0 K/s",384,0.3,0,1,&change), "LED transition midpoint");
    Require(((renderer.Pixels()[brightY * renderer.PixelSize().cx + brightX] >> 8) & 255) < 100,
        "changing character extinguishes to dark lamps at midpoint");
    for(int y=0;y<renderer.PixelSize().cy;++y)
        for(int x=0;x<static_cast<int>((27+(94.3f-20)*(37.0f*12/11/24))*4);++x)
            Require(renderer.Pixels()[y*renderer.PixelSize().cx+x] == oldReading[y*renderer.PixelSize().cx+x],
                "unchanged characters, chart and ring stay stable during LED switching");
    change.progress = 1;
    Require(renderer.Render(L"129.0 K/s",L"129.0 K/s",384,0.3,0,1,&change), "LED transition end");
    const auto settled = renderer.Pixels();
    Require(renderer.Render(L"129.0 K/s",L"129.0 K/s",384,0.3,0), "latest value");
    Require(renderer.Pixels() == settled, "LED transition ends at the exact latest reading");
    const auto evidence = argc > 1 ? std::filesystem::path(argv[1]) : std::filesystem::path{};
    if (!evidence.empty())
    {
        std::filesystem::create_directories(evidence);
        for(int frame=0;frame<=20;++frame)
        {
            change.progress = frame / 20.0f;
            Require(renderer.Render(L"129.0 K/s",L"129.0 K/s",144,0.3,0,1,&change), "LED animation evidence");
            SaveBitmap(renderer,evidence/("led-"+std::to_string(frame)+".bmp"),0x101419);
        }
    }
    Require(renderer.Render(L"12.3 K/s",L"45.6 M/s",96,0.3,0.5,0,nullptr,0), "circle-only render");
    Require(renderer.PixelSize().cx == 54 && renderer.PixelSize().cy == 54, "circle canvas dimensions");
    Require(!FloatingRateRenderer::HitTest({50,5},96,0,0) && FloatingRateRenderer::HitTest({27,27},96,0,0), "circle hit shape");
    if (!evidence.empty()) SaveBitmap(renderer,evidence/"circle-only.bmp",0x101419);
    const auto circlePixels = renderer.Pixels();
    Require(renderer.Render(L"999.9 G/s",L"999.9 M/s",96,0.3,0.5,1,nullptr,0), "text-only render");
    for (int y = 7; y <= 47; ++y)
        for (int x = 7; x <= 47; ++x)
            if ((x-27)*(x-27)+(y-27)*(y-27) <= 420)
                Require(renderer.Pixels()[y*renderer.PixelSize().cx+x] == circlePixels[y*54+x],
                    "text-only lamps do not overlap the unchanged ring and wave");
    Require(renderer.PixelSize().cx >= 149, "text-only width includes ring-to-text clearance");
    Require(renderer.Render(L"0.7 K/s",L"0.6 K/s",144,0.3,0.5,1,nullptr,0), "text-only evidence");
    if (!evidence.empty()) SaveBitmap(renderer,evidence/"text-only.bmp",0x101419);
    for(float charts : {0.0f,0.5f,1.0f})
        for(float text : {0.0f,0.5f,1.0f})
            Require(renderer.Render(L"12.3 K/s",L"45.6 M/s",144,0.3,0.5,text,nullptr,charts), "independent visibility combinations");
    renderer.ResetHistory();
    for(int i=0;i<32;++i) renderer.AddSample(300000+(i%5)*70000, 700000+(i%7)*140000);
    for (UINT dpi : {96u,120u,144u,192u})
    {
        Require(renderer.Render(L"999.9 G/s", L"999.9 M/s", dpi), "DPI evidence");
        for (DWORD pixel : renderer.Pixels())
        {
            const DWORD alpha = pixel >> 24;
            Require((pixel & 255) <= alpha && ((pixel >> 8) & 255) <= alpha && ((pixel >> 16) & 255) <= alpha, "premultiplied output");
        }
        if (!evidence.empty())
            for (bool light : {false,true}) SaveBitmap(renderer, evidence / (std::to_string(dpi) + (light ? "-light.bmp" : "-dark.bmp")), light ? 0xf4f5f7 : 0x101419);
    }
}
