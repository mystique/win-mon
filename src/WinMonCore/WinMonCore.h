#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace winmon
{
struct NicSnapshot final
{
    std::string stableId;
    std::wstring name;
    bool loopback = false;
    bool up = false;
    std::uint64_t inOctets = 0;
    std::uint64_t outOctets = 0;
};

struct RateDisplay final
{
    double uploadBytesPerSecond = 0.0;
    double downloadBytesPerSecond = 0.0;
    std::wstring uploadText;
    std::wstring downloadText;
};

class WinMonCore final
{
public:
    RateDisplay Sample(const std::vector<NicSnapshot>& snapshots, double monotonicSeconds);
    static std::wstring FormatRate(double bytesPerSecond);

private:
    struct PreviousSample final
    {
        std::string stableId;
        std::uint64_t inOctets = 0;
        std::uint64_t outOctets = 0;
    };
    std::vector<PreviousSample> previous_;
    double previousTime_ = 0.0;
    bool hasPrevious_ = false;
};
}
