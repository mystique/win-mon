#include "NetworkObservationReader.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main()
{
    try
    {
        using Clock = std::chrono::steady_clock;
        auto now = Clock::time_point{};
        MIB_IF_ROW2 row{};
        row.InterfaceGuid.Data1 = 1;
        row.InterfaceLuid.Value = 42;
        row.OperStatus = IfOperStatusUp;
        row.InterfaceAndOperStatusFlags.HardwareInterface = TRUE;
        row.InOctets = 100;
        row.OutOctets = 200;
        wcscpy_s(row.Alias, L"Ethernet");
        std::optional<std::vector<MIB_IF_ROW2>> rows = std::vector<MIB_IF_ROW2>{row};
        ClassicConnectionIds classification{true, {row.InterfaceGuid}};
        int reads = 0;
        NetworkObservationReader reader({[&] { return rows; }, [&] { ++reads; return classification; }, [&] { return now; }});
        auto observation = reader.Read();
        Require(reads == 1 && observation.classicClassificationAvailable && observation.snapshots.size() == 1,
            "first read queries classification even at clock epoch");
        const auto& nic = observation.snapshots.front();
        Require(nic.stableId == "42" && nic.friendlyName == L"Ethernet" && nic.up && nic.hardwareInterface &&
            nic.visibleInClassicConnections && nic.inOctets == 100 && nic.outOctets == 200, "native row maps to observation");
        now += std::chrono::seconds(29);
        rows->front().InOctets = 500;
        observation = reader.Read();
        Require(reads == 1 && observation.snapshots.front().inOctets == 500 && observation.sampledAt == now,
            "fresh counters reuse classification before 30 seconds");
        now += std::chrono::seconds(1);
        static_cast<void>(reader.Read());
        Require(reads == 2, "classification refreshes at 30 seconds");
        row.InterfaceGuid.Data1 = 2;
        row.InterfaceLuid.Value = 43;
        rows->push_back(row);
        observation = reader.Read();
        Require(reads == 3 && observation.snapshots.size() == 2 && !observation.snapshots.back().visibleInClassicConnections,
            "topology change immediately refreshes and classifies new NIC");
        std::reverse(rows->begin(), rows->end());
        static_cast<void>(reader.Read());
        Require(reads == 3, "row order does not invalidate classification");
        classification = {};
        now += std::chrono::seconds(30);
        observation = reader.Read();
        Require(reads == 4 && !observation.classicClassificationAvailable && observation.snapshots.size() == 2,
            "classification failure keeps counters and exposes fallback");
        now += std::chrono::seconds(1);
        static_cast<void>(reader.Read());
        Require(reads == 4, "classification failure waits before retry");
        classification = {true, {row.InterfaceGuid}};
        now += std::chrono::seconds(1);
        observation = reader.Read();
        Require(reads == 5 && observation.classicClassificationAvailable && observation.snapshots.front().visibleInClassicConnections,
            "classification recovers at two-second retry");
        rows.reset();
        observation = reader.Read();
        Require(reads == 5 && observation.snapshots.empty() && !observation.classicClassificationAvailable && observation.sampledAt == now,
            "table failure produces empty observation without querying classification");
        rows = std::vector<MIB_IF_ROW2>{};
        observation = reader.Read();
        Require(reads == 6 && observation.snapshots.empty(), "NIC removal invalidates classification");
        std::cout << "Network observation checks passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
