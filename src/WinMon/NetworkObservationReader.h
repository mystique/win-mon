#pragma once
#include <windows.h>
#include <ws2def.h>
#include <ws2ipdef.h>
#include <iphlpapi.h>
#include "../WinMonCore/WinMonCore.h"
#include <functional>
#include <optional>

struct ClassicConnectionIds final
{
    bool available = false;
    std::vector<GUID> values;
};

// Owns Windows reads and the classification cache used by sampling and menus.
class NetworkObservationReader final
{
public:
    struct Sources final
    {
        std::function<std::optional<std::vector<MIB_IF_ROW2>>()> rows;
        std::function<ClassicConnectionIds()> classification;
        std::function<std::chrono::steady_clock::time_point()> now;
    };
    NetworkObservationReader();
    explicit NetworkObservationReader(Sources sources);
    [[nodiscard]] winmon::NetworkObservation Read();

private:
    Sources sources_;
    std::vector<GUID> observedInterfaces_;
    ClassicConnectionIds classification_;
    std::chrono::steady_clock::time_point classificationRefreshAt_{};
    bool hasClassification_ = false;
};
