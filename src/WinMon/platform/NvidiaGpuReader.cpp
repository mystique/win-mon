// SPDX-License-Identifier: MIT
#include "platform/NvidiaGpuReader.h"

struct NvidiaGpuReader::Thermal
{
    unsigned version, count;
    struct Sensor { unsigned controller, minimum, maximum, current, target; } sensors[3];
};

struct NvidiaGpuReader::Pstates
{
    unsigned version, flags;
    struct Domain { unsigned flags, percentage; } domains[8];
};

NvidiaGpuReader::~NvidiaGpuReader()
{
    if (unload_) unload_();
    if (module_) FreeLibrary(module_);
}

std::optional<GpuSample> NvidiaGpuReader::Probe()
{
    if (module_) return Read();
    module_ = LoadLibraryExW(L"nvapi64.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (!module_) return {};
    const auto query = reinterpret_cast<void* (*)(unsigned)>(GetProcAddress(module_, "nvapi_QueryInterface"));
    if (!query) return {};
    const auto init = reinterpret_cast<int (*)()>(query(0x0150E828));
    const auto unload = reinterpret_cast<int (*)()>(query(0xD22BDD7E));
    const auto enumerate = reinterpret_cast<int (*)(void**, unsigned*)>(query(0xE5AC921F));
    thermal_ = reinterpret_cast<decltype(thermal_)>(query(0xE3640A56));
    pstates_ = reinterpret_cast<decltype(pstates_)>(query(0x60DED2ED));
    if (!init || !unload || !enumerate || !thermal_ || !pstates_ || init() != 0) return {};
    unload_ = unload;
    void* devices[64]{};
    unsigned count = 0;
    if (enumerate(devices, &count) != 0 || count > 64) return {};
    for (unsigned i = 0; i < count; ++i)
    {
        gpu_ = devices[i];
        if (auto sample = Read()) return sample;
    }
    gpu_ = nullptr;
    return {};
}

std::optional<GpuSample> NvidiaGpuReader::Read() const
{
    static_assert(sizeof(Thermal) == 68 && sizeof(Pstates) == 72);
    if (!gpu_) return {};
    Thermal temperature{sizeof(Thermal) | (1u << 16)};
    Pstates load{sizeof(Pstates) | (1u << 16)};
    if (thermal_(gpu_, 0, &temperature) != 0 || temperature.count != 1 ||
        temperature.sensors[0].target != 1 || pstates_(gpu_, &load) != 0 ||
        !(load.domains[0].flags & 1) || load.domains[0].percentage > 100) return {};
    return GpuSample{load.domains[0].percentage, temperature.sensors[0].current};
}
