#include "support/CoreFixtures.h"

using namespace winmon;
using namespace test;

TEST_CASE("Selecting a NIC excludes other traffic and All restores aggregation", "[unit][network]")
{
    WinMonCore core;
    auto a = NamedNic("a", L"Ethernet", L""), b = NamedNic("b", L"Wi-Fi", L"");
    core.ObserveNetwork({{a, b}, true, RateSampleAt(1)});
    core.Sample();
    auto menu = core.BeginOperatorMenu();
    REQUIRE(menu.has_value());
    core.CompleteOperatorMenu(FindItem(*menu, L"Ethernet").choiceToken);
    a.inOctets = 1000;
    a.outOctets = 2000;
    b.inOctets = 50000;
    b.outOctets = 70000;
    core.ObserveNetwork({{a, b}, true, RateSampleAt(2)});
    const auto selected = core.Sample();
    RequireRate(selected, 2000, 1000);
    menu = core.BeginOperatorMenu();
    REQUIRE(menu.has_value());
    core.CompleteOperatorMenu(FindItem(*menu, L"All").choiceToken);
    a.inOctets += 3000;
    a.outOctets += 4000;
    b.inOctets += 5000;
    b.outOctets += 6000;
    core.ObserveNetwork({{a, b}, true, RateSampleAt(3)});
    const auto all = core.Sample();
    RequireRate(all, 10000, 8000);
    CHECK(all.networkGeneration == selected.networkGeneration + 1);
}

TEST_CASE("A newly discovered NIC establishes a baseline before contributing traffic", "[unit][network]")
{
    WinMonCore core;
    core.ObserveNetwork({{Nic("a", true, 100, 100)}, true, RateSampleAt(1)});
    core.Sample();
    core.ObserveNetwork({{Nic("a", true, 200, 300), Nic("b", true, 9000000, 8000000)}, true, RateSampleAt(2)});
    RequireRate(core.Sample(), 200, 100);
    core.ObserveNetwork({{Nic("a", true, 300, 500), Nic("b", true, 9001000, 8002000)}, true, RateSampleAt(3)});
    RequireRate(core.Sample(), 2200, 1100);
}

TEST_CASE("A selected disconnected NIC resumes from its latest baseline", "[unit][network]")
{
    WinMonCore core;
    auto nic = NamedNic("a", L"Ethernet", L"");
    core.ObserveNetwork({{nic}, true, RateSampleAt(1)});
    core.Sample();
    auto menu = core.BeginOperatorMenu();
    REQUIRE(menu.has_value());
    core.CompleteOperatorMenu(FindItem(*menu, L"Ethernet").choiceToken);
    nic.up = false;
    nic.inOctets = 100000;
    nic.outOctets = 200000;
    core.ObserveNetwork({{nic}, true, RateSampleAt(2)});
    RequireRate(core.Sample(), 0, 0);
    nic.up = true;
    nic.inOctets += 1000;
    nic.outOctets += 2000;
    core.ObserveNetwork({{nic}, true, RateSampleAt(3)});
    RequireRate(core.Sample(), 2000, 1000);
}

TEST_CASE("Numeric LED cells retain zero rounding and overflow behavior", "[unit][format]")
{
    CHECK((WinMonCore::FormatRateCells(0, L'\u2191') ==
           std::array<wchar_t, 9>{L'\u2191', L' ', L' ', L'0', L'.', L'0', L'K', L'/', L's'}));
    CHECK((WinMonCore::FormatRateCells(12345, L'\u2193') ==
           std::array<wchar_t, 9>{L'\u2193', L' ', L'1', L'2', L'.', L'3', L'K', L'/', L's'}));
    CHECK((WinMonCore::FormatRateCells(999999999, L'\u2191') ==
           std::array<wchar_t, 9>{L'\u2191', L'9', L'9', L'9', L'.', L'9', L'M', L'/', L's'}));
}

TEST_CASE("Network selection and sampling share one canonical observation", "[unit][network]")
{
    WinMonCore core;
    auto hidden = NamedNic("internal", L"Internal", L"Internal transport");
    hidden.visibleInClassicConnections = false;
    core.ObserveNetwork({{NamedNic("up", L"Ethernet", L"Intel"), NamedNic("down", L"", L"Disconnected", false), hidden},
                         true,
                         RateSampleAt(1.0)});
    core.Sample();

    REQUIRE(core.Sample().networkGeneration == 0);
    const auto opened = core.BeginOperatorMenu({true, false});
    REQUIRE(opened.has_value());
    const auto& down = FindItem(*opened, L"Disconnected");
    REQUIRE(FindItem(*opened, L"All").checked);
    REQUIRE(FindItemPointer(*opened, L"Internal") == nullptr);
    REQUIRE(!core.BeginOperatorMenu({}).has_value());
    REQUIRE(core.CompleteOperatorMenu(down.choiceToken).kind == OperatorActionKind::None);

    REQUIRE(core.Sample().networkGeneration == 1);
    auto observedUp = NamedNic("up", L"Ethernet", L"Intel");
    observedUp.inOctets = 1000;
    observedUp.outOctets = 1000;
    auto observedDown = NamedNic("down", L"", L"Disconnected", false);
    observedDown.inOctets = 2000;
    observedDown.outOctets = 3000;
    core.ObserveNetwork({{observedUp, observedDown}, true, RateSampleAt(2.0)});
    RequireRate(core.Sample(), 0.0, 0.0);
    const auto downMenu = core.BeginOperatorMenu({});
    REQUIRE(downMenu.has_value());
    REQUIRE(FindItem(*downMenu, L"Disconnected").checked);
    core.CancelOperatorMenu();

    observedUp.inOctets = 2000;
    observedUp.outOctets = 4000;
    core.ObserveNetwork({{observedUp}, true, RateSampleAt(3.0)});
    RequireRate(core.Sample(), 3000.0, 1000.0);
    REQUIRE(core.Sample().networkGeneration == 2);
    const auto fallbackMenu = core.BeginOperatorMenu({});
    REQUIRE(fallbackMenu.has_value());
    REQUIRE(FindItem(*fallbackMenu, L"All").checked);
    core.CancelOperatorMenu();
}

TEST_CASE("Classification fallback preserves aggregate and selected NIC rates", "[unit][network]")
{
    WinMonCore core;
    auto otherwiseHidden = NamedNic("internal", L"Internal", L"Internal transport");
    otherwiseHidden.visibleInClassicConnections = false;
    core.ObserveNetwork({{otherwiseHidden}, false, RateSampleAt(1.0)});
    core.Sample();

    otherwiseHidden.inOctets = 1000;
    otherwiseHidden.outOctets = 2000;
    core.ObserveNetwork({{otherwiseHidden}, false, RateSampleAt(2.0)});
    RequireRate(core.Sample(), 2000.0, 1000.0);

    const auto menu = core.BeginOperatorMenu({});
    REQUIRE(menu.has_value());
    const auto& internal = FindItem(*menu, L"Internal");
    REQUIRE(core.CompleteOperatorMenu(internal.choiceToken).kind == OperatorActionKind::None);

    otherwiseHidden.inOctets = 4000;
    otherwiseHidden.outOctets = 6000;
    core.ObserveNetwork({{otherwiseHidden}, true, RateSampleAt(3.0)});
    RequireRate(core.Sample(), 4000.0, 3000.0);
}

TEST_CASE("The first sample reports zero rates", "[unit][network]")
{
    WinMonCore core;
    core.ObserveNetwork({{Nic("a", true, 100, 200)}, false, RateSampleAt(10.0)});
    const auto display = core.Sample();
    RequireRate(display, 0.0, 0.0);
    REQUIRE((display.uploadText == L"0.0 K/s" && display.downloadText == L"0.0 K/s"));
}

TEST_CASE("Rates use the counter capture interval", "[unit][network]")
{
    WinMonCore core;
    core.ObserveNetwork({{Nic("a", true, 1000, 2000)}, false, RateSampleAt(3.0)});
    core.Sample();
    core.ObserveNetwork({{Nic("a", true, 5000, 5000)}, false, RateSampleAt(5.0)});
    const auto display = core.Sample();
    RequireRate(display, 1500.0, 2000.0);
}

TEST_CASE("All aggregates only visible connected hardware NICs", "[unit][network]")
{
    WinMonCore core;
    auto hidden = Nic("hidden", true, 0, 0);
    hidden.visibleInClassicConnections = false;
    core.ObserveNetwork(
        {{Nic("physical-a", true, 0, 0), Nic("physical-b", true, 0, 0), Nic("virtual", true, 0, 0, false, false),
          Nic("down", false, 0, 0), Nic("loop", true, 0, 0, true), hidden},
         true,
         RateSampleAt(1.0)});
    core.Sample();

    hidden.inOctets = 500000;
    hidden.outOctets = 600000;
    core.ObserveNetwork({{Nic("physical-a", true, 1000, 2000), Nic("physical-b", true, 3000, 7000),
                          Nic("virtual", true, 700000, 800000, false, false), Nic("down", false, 100000, 100000),
                          Nic("loop", true, 900000, 900000, true), hidden},
                         true,
                         RateSampleAt(2.0)});
    const auto display = core.Sample();
    RequireRate(display, 9000.0, 4000.0);
}

TEST_CASE("A selected virtual NIC reports its own traffic", "[unit][network]")
{
    WinMonCore core;
    auto virtualNic = NamedNic("virtual", L"Meta", L"Meta Tunnel");
    virtualNic.hardwareInterface = false;
    core.ObserveNetwork({{virtualNic}, true, RateSampleAt(1.0)});
    RequireRate(core.Sample(), 0.0, 0.0);

    const auto menu = core.BeginOperatorMenu();
    REQUIRE(menu.has_value());
    const auto& meta = FindItem(*menu, L"Meta");
    core.CompleteOperatorMenu(meta.choiceToken);

    virtualNic.inOctets = 3000;
    virtualNic.outOctets = 5000;
    core.ObserveNetwork({{virtualNic}, true, RateSampleAt(2.0)});
    RequireRate(core.Sample(), 5000.0, 3000.0);
}

TEST_CASE("Missing and disconnected networks report zero rates", "[unit][network]")
{
    WinMonCore core;
    core.ObserveNetwork({{Nic("a", true, 10, 20)}, false, RateSampleAt(1.0)});
    core.Sample();
    core.ObserveNetwork({{}, false, RateSampleAt(2.0)});
    auto display = core.Sample();
    RequireRate(display, 0.0, 0.0);
    core.ObserveNetwork({{Nic("a", false, 20, 30)}, false, RateSampleAt(3.0)});
    display = core.Sample();
    RequireRate(display, 0.0, 0.0);
}

TEST_CASE("Counter rollback and NIC removal do not produce negative rates", "[unit][network]")
{
    WinMonCore core;
    core.ObserveNetwork({{Nic("a", true, 100, 100), Nic("gone", true, 100, 100)}, false, RateSampleAt(1.0)});
    core.Sample();
    core.ObserveNetwork({{Nic("a", true, 50, 200)}, false, RateSampleAt(2.0)});
    auto display = core.Sample();
    RequireRate(display, 100.0, 0.0);
    core.ObserveNetwork({{Nic("a", true, 150, 300)}, false, RateSampleAt(3.0)});
    display = core.Sample();
    RequireRate(display, 100.0, 100.0);
}

TEST_CASE("Nonpositive sampling intervals report zero rates", "[unit][network]")
{
    WinMonCore core;
    core.ObserveNetwork({{Nic("a", true, 100, 100)}, false, RateSampleAt(4.0)});
    core.Sample();
    core.ObserveNetwork({{Nic("a", true, 200, 200)}, false, RateSampleAt(4.0)});
    auto display = core.Sample();
    RequireRate(display, 0.0, 0.0);
    core.ObserveNetwork({{Nic("a", true, 300, 300)}, false, RateSampleAt(3.0)});
    display = core.Sample();
    RequireRate(display, 0.0, 0.0);
}

TEST_CASE("Rates use base 1000 units and a locale-independent decimal point", "[unit][network]")
{
    REQUIRE(WinMonCore::FormatRate(0.0) == L"0.0 K/s");
    REQUIRE(WinMonCore::FormatRate(999.0) == L"1.0 K/s");
    REQUIRE(WinMonCore::FormatRate(1000.0) == L"1.0 K/s");
    REQUIRE(WinMonCore::FormatRate(12345.0) == L"12.3 K/s");
    REQUIRE(WinMonCore::FormatRate(1000000.0) == L"1.0 M/s");
    REQUIRE(WinMonCore::FormatRate(999999999.0) == L"1000.0 M/s");
    REQUIRE(WinMonCore::FormatRate(1000000000.0) == L"1.0 G/s");
    REQUIRE(WinMonCore::FormatRate(-1.0).find(L"B") == std::wstring::npos);
    const test::LocaleRestore restoreLocale;
    const std::locale previousLocale = std::locale();
    std::locale::global(std::locale(previousLocale, new CommaDecimalPunctuation));
    REQUIRE(WinMonCore::FormatRate(12345.0) == L"12.3 K/s");
}
