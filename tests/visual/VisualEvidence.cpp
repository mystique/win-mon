#include "floating/FloatingRateRenderer.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>

namespace
{
void SaveBitmap(const FloatingRateRenderer& renderer, const std::filesystem::path& path, DWORD background)
{
    const SIZE size = renderer.PixelSize();
    BITMAPFILEHEADER file{};
    file.bfType = 0x4d42;
    file.bfOffBits = sizeof(file) + sizeof(BITMAPINFOHEADER);
    file.bfSize = file.bfOffBits + size.cx * size.cy * 4;
    BITMAPINFOHEADER info{sizeof(info), size.cx, -size.cy, 1, 32, BI_RGB};
    std::ofstream output(path, std::ios::binary);
    output.write(reinterpret_cast<const char*>(&file), sizeof(file));
    output.write(reinterpret_cast<const char*>(&info), sizeof(info));
    for (DWORD pixel : renderer.Pixels())
    {
        DWORD result = 0xff000000;
        for (int shift : {0, 8, 16})
            result |= (((pixel >> shift) & 255) + ((background >> shift) & 255) * (255 - (pixel >> 24)) / 255) << shift;
        output.write(reinterpret_cast<const char*>(&result), sizeof(result));
    }
    if (!output.good())
        throw std::runtime_error("Cannot write visual evidence");
}
} // namespace

int main(int argc, char** argv)
{
    try
    {
        if (argc != 2)
            throw std::runtime_error("Usage: WinMonVisualEvidence <output-directory>");
        const std::filesystem::path directory(argv[1]);
        std::filesystem::create_directories(directory);
        FloatingRateRenderer renderer;
        for (int i = 0; i < 32; ++i)
            renderer.AddSample(300000 + (i % 5) * 70000, 700000 + (i % 7) * 140000);
        for (UINT dpi : {96u, 120u, 144u, 192u})
            for (bool charts : {false, true})
                for (bool text : {false, true})
                {
                    if (!renderer.Render(L"999.9 G/s", L"12.8 M/s", dpi, 0.3, 0.5, text ? 1.0f : 0.0f, nullptr,
                                         charts ? 1.0f : 0.0f))
                        throw std::runtime_error("Cannot render evidence frame");
                    for (bool light : {false, true})
                        SaveBitmap(renderer,
                                   directory / (std::to_string(dpi) + (charts ? "-charts" : "-no-charts") +
                                                (text ? "-text" : "-no-text") + (light ? "-light.bmp" : "-dark.bmp")),
                                   light ? 0xf4f5f7 : 0x101419);
                }
        for (int frame = 0; frame < 30; ++frame)
        {
            if (!renderer.Render(L"12.3 K/s", L"45.6 M/s", 144, frame / 30.0, 0.5, 1, nullptr, 1, frame / 30.0f))
                throw std::runtime_error("Cannot render animation evidence");
            SaveBitmap(renderer, directory / ("scroll-" + std::to_string(frame) + ".bmp"), 0x101419);
        }
        std::cout << "Visual evidence: " << directory << '\n';
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
