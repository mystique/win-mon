#include "platform/NetworkObservationReader.h"
#include <catch2/catch_test_macros.hpp>
#include <algorithm>

TEST_CASE("Network observation caches classification and retries failures", "[unit][network-reader]")
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
    NetworkObservationReader reader({[&] { return rows; },
                                     [&] {
                                         ++reads;
                                         return classification;
                                     },
                                     [&] { return now; }});
    auto observation = reader.Read();
    REQUIRE((reads == 1 && observation.classicClassificationAvailable && observation.snapshots.size() == 1));
    const auto& nic = observation.snapshots.front();
    REQUIRE((nic.stableId == "42" && nic.friendlyName == L"Ethernet" && nic.up && nic.hardwareInterface &&
             nic.visibleInClassicConnections && nic.inOctets == 100 && nic.outOctets == 200));
    now += std::chrono::seconds(29);
    rows->front().InOctets = 500;
    observation = reader.Read();
    REQUIRE((reads == 1 && observation.snapshots.front().inOctets == 500 && observation.sampledAt == now));
    now += std::chrono::seconds(1);
    static_cast<void>(reader.Read());
    REQUIRE(reads == 2);
    row.InterfaceGuid.Data1 = 2;
    row.InterfaceLuid.Value = 43;
    rows->push_back(row);
    observation = reader.Read();
    REQUIRE(
        (reads == 3 && observation.snapshots.size() == 2 && !observation.snapshots.back().visibleInClassicConnections));
    std::reverse(rows->begin(), rows->end());
    static_cast<void>(reader.Read());
    REQUIRE(reads == 3);
    classification = {};
    now += std::chrono::seconds(30);
    observation = reader.Read();
    REQUIRE((reads == 4 && !observation.classicClassificationAvailable && observation.snapshots.size() == 2));
    now += std::chrono::seconds(1);
    static_cast<void>(reader.Read());
    REQUIRE(reads == 4);
    classification = {true, {row.InterfaceGuid}};
    now += std::chrono::seconds(1);
    observation = reader.Read();
    REQUIRE((reads == 5 && observation.classicClassificationAvailable &&
             observation.snapshots.front().visibleInClassicConnections));
    rows.reset();
    observation = reader.Read();
    REQUIRE((reads == 5 && observation.snapshots.empty() && !observation.classicClassificationAvailable &&
             observation.sampledAt == now));
    rows = std::vector<MIB_IF_ROW2>{};
    observation = reader.Read();
    REQUIRE((reads == 6 && observation.snapshots.empty()));
}
