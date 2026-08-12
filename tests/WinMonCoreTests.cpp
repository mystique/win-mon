#include "../src/WinMonCore/WinMonCore.h"

#include <cmath>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using winmon::NicSnapshot;
using winmon::WinMonCore;

NicSnapshot Nic(const char* id, bool up, std::uint64_t inOctets, std::uint64_t outOctets, bool loopback = false)
{
    NicSnapshot nic;
    nic.stableId = id;
    nic.up = up;
    nic.loopback = loopback;
    nic.inOctets = inOctets;
    nic.outOctets = outOctets;
    return nic;
}

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

void RequireRate(const winmon::RateDisplay& display, double upload, double download)
{
    if (std::abs(display.uploadBytesPerSecond - upload) >= 0.001 ||
        std::abs(display.downloadBytesPerSecond - download) >= 0.001)
    {
        throw std::runtime_error(
            "rate mismatch: expected upload=" + std::to_string(upload) +
            " download=" + std::to_string(download) +
            ", actual upload=" + std::to_string(display.uploadBytesPerSecond) +
            " download=" + std::to_string(display.downloadBytesPerSecond));
    }
}

void FirstSampleIsZero()
{
    WinMonCore core;
    const auto display = core.Sample({Nic("a", true, 100, 200)}, 10.0);
    RequireRate(display, 0.0, 0.0);
    Require(display.uploadText == L"0.0K/s" && display.downloadText == L"0.0K/s", "first sample formatting");
}

void SingleNicUsesActualElapsedTimeAndCounterDirections()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 1000, 2000)}, 3.0);
    const auto display = core.Sample({Nic("a", true, 5000, 5000)}, 5.0);
    RequireRate(display, 1500.0, 2000.0);
}

void AllNicsSumsOnlyUpNonLoopback()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 0, 0), Nic("b", true, 0, 0), Nic("down", false, 0, 0), Nic("loop", true, 0, 0, true)}, 1.0);
    const auto display = core.Sample({Nic("a", true, 1000, 2000), Nic("b", true, 3000, 7000), Nic("down", false, 100000, 100000), Nic("loop", true, 900000, 900000, true)}, 2.0);
    RequireRate(display, 9000.0, 4000.0);
}

void EmptyAndNoUpAreZero()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 10, 20)}, 1.0);
    auto display = core.Sample({}, 2.0);
    RequireRate(display, 0.0, 0.0);
    display = core.Sample({Nic("a", false, 20, 30)}, 3.0);
    RequireRate(display, 0.0, 0.0);
}

void BackwardCountersAndDisappearingNicsAreZero()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 100, 100), Nic("gone", true, 100, 100)}, 1.0);
    auto display = core.Sample({Nic("a", true, 50, 200)}, 2.0);
    RequireRate(display, 0.0, 0.0);
    display = core.Sample({Nic("a", true, 150, 300)}, 3.0);
    RequireRate(display, 100.0, 100.0);
}

void NonpositiveElapsedIsZero()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 100, 100)}, 4.0);
    auto display = core.Sample({Nic("a", true, 200, 200)}, 4.0);
    RequireRate(display, 0.0, 0.0);
    display = core.Sample({Nic("a", true, 300, 300)}, 3.0);
    RequireRate(display, 0.0, 0.0);
}

void FormatsBase1000BoundariesAndMinimumK()
{
    Require(WinMonCore::FormatRate(0.0) == L"0.0K/s", "zero format");
    Require(WinMonCore::FormatRate(999.0) == L"1.0K/s", "sub kilo format");
    Require(WinMonCore::FormatRate(1000.0) == L"1.0K/s", "K boundary");
    Require(WinMonCore::FormatRate(12345.0) == L"12.3K/s", "K rounding");
    Require(WinMonCore::FormatRate(1000000.0) == L"1.0M/s", "M boundary");
    Require(WinMonCore::FormatRate(999999999.0) == L"1000.0M/s", "M upper format");
    Require(WinMonCore::FormatRate(1000000000.0) == L"1.0G/s", "G boundary");
    Require(WinMonCore::FormatRate(-1.0).find(L"B") == std::wstring::npos, "no B unit");
}
}

int main()
{
    try
    {
        FirstSampleIsZero();
        SingleNicUsesActualElapsedTimeAndCounterDirections();
        AllNicsSumsOnlyUpNonLoopback();
        EmptyAndNoUpAreZero();
        BackwardCountersAndDisappearingNicsAreZero();
        NonpositiveElapsedIsZero();
        FormatsBase1000BoundariesAndMinimumK();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "WinMonCore tests passed\n";
    return 0;
}
