// SPDX-License-Identifier: MIT
#pragma once

#include <windows.h>
#include <optional>

struct GpuSample
{
    unsigned load;
    unsigned celsius;
};

class NvidiaGpuReader final
{
public:
    NvidiaGpuReader() = default;
    NvidiaGpuReader(const NvidiaGpuReader&) = delete;
    NvidiaGpuReader& operator=(const NvidiaGpuReader&) = delete;
    ~NvidiaGpuReader();
    std::optional<GpuSample> Probe();
    std::optional<GpuSample> Read() const;
    bool Available() const { return gpu_ != nullptr; }

private:
    struct Thermal;
    struct Pstates;
    HMODULE module_ = nullptr;
    void* gpu_ = nullptr;
    int (*unload_)() = nullptr;
    int (*thermal_)(void*, unsigned, Thermal*) = nullptr;
    int (*pstates_)(void*, Pstates*) = nullptr;
};
