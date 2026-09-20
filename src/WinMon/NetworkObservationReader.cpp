#include "NetworkObservationReader.h"
#include <iphlpapi.h>
#include <netcon.h>
#include <wrl/client.h>
#include <algorithm>
#include <memory>
#include <utility>

namespace
{
std::optional<std::vector<MIB_IF_ROW2>> ReadRows()
{
    MIB_IF_TABLE2* raw = nullptr;
    const DWORD status = GetIfTable2(&raw);
    const std::unique_ptr<MIB_IF_TABLE2, decltype(&FreeMibTable)> table(raw, FreeMibTable);
    if (status != NO_ERROR || !table) return std::nullopt;
    return std::vector<MIB_IF_ROW2>(table->Table, table->Table + table->NumEntries);
}
ClassicConnectionIds ReadClassicConnectionIds()
{
    const HRESULT initialization = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    const bool shouldUninitialize = SUCCEEDED(initialization);
    if (FAILED(initialization) && initialization != RPC_E_CHANGED_MODE)
    {
        return {};
    }

    ClassicConnectionIds result;
    {
        Microsoft::WRL::ComPtr<INetConnectionManager> manager;
        if (SUCCEEDED(CoCreateInstance(
                CLSID_ConnectionManager,
                nullptr,
                CLSCTX_LOCAL_SERVER,
                IID_PPV_ARGS(&manager))))
        {
            Microsoft::WRL::ComPtr<IEnumNetConnection> connections;
            if (SUCCEEDED(manager->EnumConnections(NCME_DEFAULT, &connections)))
            {
                result.available = true;
                Microsoft::WRL::ComPtr<INetConnection> connection;
                ULONG fetched = 0;
                while (connections->Next(1, connection.ReleaseAndGetAddressOf(), &fetched) == S_OK)
                {
                    NETCON_PROPERTIES* properties = nullptr;
                    if (SUCCEEDED(connection->GetProperties(&properties)) && properties != nullptr)
                    {
                        result.values.push_back(properties->guidId);
                        CoTaskMemFree(properties->pszwName);
                        CoTaskMemFree(properties->pszwDeviceName);
                        CoTaskMemFree(properties);
                    }
                }
            }
        }
    }

    if (shouldUninitialize)
    {
        CoUninitialize();
    }
    return result;
}

bool ContainsConnectionId(const std::vector<GUID>& ids, const GUID& candidate) noexcept
{
    return std::any_of(ids.begin(), ids.end(), [&candidate](const GUID& id) {
        return InlineIsEqualGUID(id, candidate) != FALSE;
    });
}

}

NetworkObservationReader::NetworkObservationReader()
    : NetworkObservationReader({ReadRows, ReadClassicConnectionIds, std::chrono::steady_clock::now}) {}

NetworkObservationReader::NetworkObservationReader(Sources sources) : sources_(std::move(sources)) {}

winmon::NetworkObservation NetworkObservationReader::Read()
{
    const auto rows = sources_.rows();
    const auto sampledAt = sources_.now();
    if (!rows) return {{}, false, sampledAt};
    std::vector<GUID> interfaces;
    interfaces.reserve(rows->size());
    for (const auto& row : *rows) interfaces.push_back(row.InterfaceGuid);
    std::sort(interfaces.begin(), interfaces.end(), [](const GUID& left, const GUID& right) {
        return memcmp(&left, &right, sizeof(GUID)) < 0;
    });
    const bool topologyChanged = !std::equal(interfaces.begin(), interfaces.end(),
        observedInterfaces_.begin(), observedInterfaces_.end(), [](const GUID& left, const GUID& right) {
            return InlineIsEqualGUID(left, right) != FALSE;
        });
    if (!hasClassification_ || topologyChanged || sampledAt >= classificationRefreshAt_)
    {
        classification_ = sources_.classification();
        observedInterfaces_ = std::move(interfaces);
        hasClassification_ = true;
        // ponytail: same-GUID classification changes can lag 30s; use notifications if immediate updates are needed.
        classificationRefreshAt_ = sampledAt + std::chrono::seconds(classification_.available ? 30 : 2);
    }
    std::vector<winmon::NicSnapshot> snapshots;
    snapshots.reserve(rows->size());
    for (const auto& row : *rows)
    {
        winmon::NicSnapshot snapshot;
        snapshot.stableId = std::to_string(row.InterfaceLuid.Value);
        snapshot.friendlyName = row.Alias;
        snapshot.description = row.Description;
        snapshot.loopback = row.Type == IF_TYPE_SOFTWARE_LOOPBACK;
        snapshot.visibleInClassicConnections =
            classification_.available && ContainsConnectionId(classification_.values, row.InterfaceGuid);
        snapshot.up = row.OperStatus == IfOperStatusUp;
        snapshot.hardwareInterface = row.InterfaceAndOperStatusFlags.HardwareInterface != FALSE;
        snapshot.inOctets = row.InOctets;
        snapshot.outOctets = row.OutOctets;
        snapshots.push_back(std::move(snapshot));
    }
    return {std::move(snapshots), classification_.available, sampledAt};
}
