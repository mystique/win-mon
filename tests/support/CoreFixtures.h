#pragma once

#include "WinMonCore.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>
#include <algorithm>
#include <cmath>
#include <locale>

namespace test
{
inline winmon::NicSnapshot Nic(const char* id, bool up, std::uint64_t inOctets, std::uint64_t outOctets,
                               bool loopback = false, bool hardware = true)
{
    winmon::NicSnapshot nic;
    nic.stableId = id;
    nic.up = up;
    nic.loopback = loopback;
    nic.hardwareInterface = hardware;
    nic.inOctets = inOctets;
    nic.outOctets = outOctets;
    return nic;
}

inline winmon::NicSnapshot NamedNic(const char* id, const wchar_t* friendly, const wchar_t* description, bool up = true)
{
    auto nic = Nic(id, up, 0, 0);
    nic.friendlyName = friendly;
    nic.description = description;
    return nic;
}

inline auto RateSampleAt(double seconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds))};
}

inline void RequireRate(const winmon::RateDisplay& display, double upload, double download)
{
    REQUIRE(std::isfinite(display.uploadBytesPerSecond));
    REQUIRE(std::isfinite(display.downloadBytesPerSecond));
    CHECK(display.uploadBytesPerSecond == Catch::Approx(upload).margin(0.001));
    CHECK(display.downloadBytesPerSecond == Catch::Approx(download).margin(0.001));
}

inline const winmon::OperatorMenuItem* FindItemPointer(const std::vector<winmon::OperatorMenuItem>& items,
                                                       const wchar_t* label)
{
    for (const auto& item : items)
    {
        if (item.label == label)
            return &item;
        if (const auto* nested = FindItemPointer(item.children, label))
            return nested;
    }
    return nullptr;
}

inline const winmon::OperatorMenuItem& FindItem(const std::vector<winmon::OperatorMenuItem>& items,
                                                const wchar_t* label)
{
    const auto* item = FindItemPointer(items, label);
    REQUIRE(item != nullptr);
    return *item;
}

class CommaDecimalPunctuation final : public std::numpunct<wchar_t>
{
    wchar_t do_decimal_point() const override
    {
        return L',';
    }
};

struct LocaleRestore final
{
    std::locale previous = std::locale();
    ~LocaleRestore()
    {
        std::locale::global(previous);
    }
};
} // namespace test
