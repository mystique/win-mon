#include "WinMonCore.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <sstream>

namespace winmon
{

RateDisplay WinMonCore::Sample(const std::vector<NicSnapshot>& snapshots, double monotonicSeconds)
{
    RateDisplay display;
    const double elapsed = monotonicSeconds - previousTime_;
    const bool validElapsed = hasPrevious_ && elapsed > 0.0;
    if (validElapsed)
    {
        for (const auto& current : snapshots)
        {
            if (!current.up || current.loopback)
            {
                continue;
            }

            const auto previous = std::find_if(
                previous_.begin(),
                previous_.end(),
                [&current](const PreviousSample& sample) { return sample.stableId == current.stableId; });
            if (previous == previous_.end() || current.inOctets < previous->inOctets || current.outOctets < previous->outOctets)
            {
                continue;
            }

            display.downloadBytesPerSecond += static_cast<double>(current.inOctets - previous->inOctets) / elapsed;
            display.uploadBytesPerSecond += static_cast<double>(current.outOctets - previous->outOctets) / elapsed;
        }
    }

    previous_.clear();
    previous_.reserve(snapshots.size());
    for (const auto& snapshot : snapshots)
    {
        previous_.push_back({snapshot.stableId, snapshot.inOctets, snapshot.outOctets});
    }
    previousTime_ = monotonicSeconds;
    hasPrevious_ = true;

    display.uploadText = FormatRate(display.uploadBytesPerSecond);
    display.downloadText = FormatRate(display.downloadBytesPerSecond);
    return display;
}

std::wstring WinMonCore::FormatRate(double bytesPerSecond)
{
    const double magnitude = std::max(0.0, bytesPerSecond);
    const wchar_t* suffix = L"K/s";
    double scaled = magnitude / 1000.0;
    if (magnitude >= 1'000'000'000.0)
    {
        suffix = L"G/s";
        scaled = magnitude / 1'000'000'000.0;
    }
    else if (magnitude >= 1'000'000.0)
    {
        suffix = L"M/s";
        scaled = magnitude / 1'000'000.0;
    }

    std::wostringstream text;
    text << std::fixed << std::setprecision(1) << scaled << suffix;
    return text.str();
}
}
