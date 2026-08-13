#include "WinMonCore.h"

#include <cmath>
#include <iostream>
#include <locale>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
using winmon::NicSnapshot;
using winmon::WinMonCore;

class CommaDecimalPunctuation final : public std::numpunct<wchar_t>
{
protected:
    wchar_t do_decimal_point() const override { return L','; }
};

NicSnapshot Nic(const char* id, bool up, std::uint64_t inOctets, std::uint64_t outOctets, bool loopback = false)
{
    NicSnapshot nic;
    nic.stableId = id;
    nic.friendlyName = L"";
    nic.up = up;
    nic.loopback = loopback;
    nic.inOctets = inOctets;
    nic.outOctets = outOctets;
    return nic;
}

NicSnapshot NamedNic(const char* id, const wchar_t* friendly, const wchar_t* description, bool up = true)
{
    auto nic = Nic(id, up, 0, 0);
    nic.friendlyName = friendly;
    nic.description = description;
    return nic;
}


void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void ClassicConnectionsLimitNicSelectionChoices()
{
    WinMonCore core;
    auto ethernet = NamedNic("ethernet", L"Ethernet", L"Intel Ethernet");
    auto internalAdapter = NamedNic("internal", L"Internal Adapter", L"Internal transport");
    internalAdapter.visibleInClassicConnections = false;

    const auto menu = core.BuildOperatorMenu({ethernet, internalAdapter});
    Require(menu.size() == 7, "only classic Network Connections NICs are offered");
    Require(menu[0].kind == winmon::OperatorMenuItemKind::All && menu[1].stableId == "ethernet", "classic NIC remains selectable");
    Require(menu[2].kind == winmon::OperatorMenuItemKind::Separator && menu[6].kind == winmon::OperatorMenuItemKind::Exit, "operator menu tail remains intact");
}

void RequireRate(const winmon::RateDisplay& display, double upload, double download)
{
    if (std::abs(display.uploadBytesPerSecond - upload) >= 0.001 ||
        std::abs(display.downloadBytesPerSecond - download) >= 0.001)
    {
        throw std::runtime_error(
            "rate mismatch: expected upload=" + std::to_string(upload) +
            " download=" + std::to_string(download) +
            ", actual upload=" + std::to_string(display.uploadBytesPerSecond) +
            " download=" + std::to_string(display.downloadBytesPerSecond));
    }
}

void FirstSampleIsZero()
{
    WinMonCore core;
    const auto display = core.Sample({Nic("a", true, 100, 200)}, 10.0);
    RequireRate(display, 0.0, 0.0);
    Require(display.uploadText == L"0.0K/s" && display.downloadText == L"0.0K/s", "first sample formatting");
}

void SingleNicUsesActualElapsedTimeAndCounterDirections()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 1000, 2000)}, 3.0);
    const auto display = core.Sample({Nic("a", true, 5000, 5000)}, 5.0);
    RequireRate(display, 1500.0, 2000.0);
}

void AllNicsSumsOnlyUpNonLoopback()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 0, 0), Nic("b", true, 0, 0), Nic("down", false, 0, 0), Nic("loop", true, 0, 0, true)}, 1.0);
    const auto display = core.Sample({Nic("a", true, 1000, 2000), Nic("b", true, 3000, 7000), Nic("down", false, 100000, 100000), Nic("loop", true, 900000, 900000, true)}, 2.0);
    RequireRate(display, 9000.0, 4000.0);
}

void EmptyAndNoUpAreZero()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 10, 20)}, 1.0);
    auto display = core.Sample({}, 2.0);
    RequireRate(display, 0.0, 0.0);
    display = core.Sample({Nic("a", false, 20, 30)}, 3.0);
    RequireRate(display, 0.0, 0.0);
}

void BackwardCountersAndDisappearingNicsAreZero()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 100, 100), Nic("gone", true, 100, 100)}, 1.0);
    auto display = core.Sample({Nic("a", true, 50, 200)}, 2.0);
    RequireRate(display, 100.0, 0.0);
    display = core.Sample({Nic("a", true, 150, 300)}, 3.0);
    RequireRate(display, 100.0, 100.0);
}


void MenuAndSelectionFollowEnumeration()
{
    WinMonCore core;
    const std::vector<NicSnapshot> nics = {NamedNic("a", L"Alpha", L"A desc"), NamedNic("down", L"", L"Down description", false), Nic("loop", true, 0, 0, true)};
    auto menu = core.BuildOperatorMenu(nics);
    Require(menu.size() == 8 && menu[0].label == L"All" && menu[0].checked, "default all menu");
    Require(menu[1].label == L"Alpha" && menu[2].label == L"Down description" && menu[2].checked == false, "menu order and fallback");
    Require(menu[3].kind == winmon::OperatorMenuItemKind::Separator && menu[7].kind == winmon::OperatorMenuItemKind::Exit, "menu tail");
    core.SelectNic("down");
    menu = core.BuildOperatorMenu(nics);
    Require(menu[2].checked && !menu[0].checked, "selected down checked");
    core.SelectAll();
    Require(core.IsAllSelected(), "select all");
    core.SelectNic("missing");
    menu = core.BuildOperatorMenu(nics);
    Require(core.IsAllSelected() && menu[0].checked, "missing selected NIC falls back before menu presentation");
}


void SelectedNicOnlyAndChurnFallback()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 0, 0), Nic("b", true, 0, 0)}, 1.0);
    core.SelectNic("b");
    auto display = core.Sample({Nic("a", true, 1000, 1000), Nic("b", true, 2000, 3000)}, 2.0);
    RequireRate(display, 3000.0, 2000.0);
    display = core.Sample({Nic("a", true, 2000, 2000), Nic("b", false, 3000, 5000)}, 3.0);
    RequireRate(display, 0.0, 0.0);
    display = core.Sample({Nic("a", true, 3000, 3000)}, 4.0);
    RequireRate(display, 1000.0, 1000.0);
    Require(core.IsAllSelected(), "disappeared selected falls back all");
}
void NonpositiveElapsedIsZero()
{
    WinMonCore core;
    core.Sample({Nic("a", true, 100, 100)}, 4.0);
    auto display = core.Sample({Nic("a", true, 200, 200)}, 4.0);
    RequireRate(display, 0.0, 0.0);
    display = core.Sample({Nic("a", true, 300, 300)}, 3.0);
    RequireRate(display, 0.0, 0.0);
}

void RateStripVisibilityFollowsPrimaryBottomAvailability()
{
    Require(WinMonCore::ShouldShowRateStrip(true), "primary bottom taskbar shows strip");
    Require(!WinMonCore::ShouldShowRateStrip(false), "missing or unsupported taskbar hides strip");
}

void OperatorMenuGroupsPersistedTogglesWithoutSeparator()
{
    WinMonCore core;
    const std::vector<NicSnapshot> nics = {NamedNic("a", L"Alpha", L"A desc")};
    const auto menu = core.BuildOperatorMenu(nics, {true, false});
    Require(menu.size() == 7, "toggles join the operator menu tail");
    Require(menu[3].kind == winmon::OperatorMenuItemKind::Autostart && menu[3].checked, "autostart state is checked");
    Require(menu[4].kind == winmon::OperatorMenuItemKind::RateStripContextMenu && !menu[4].checked, "rate strip toggle state is unchecked");
    Require(menu[5].kind == winmon::OperatorMenuItemKind::Separator, "only one separator before Exit");
    Require(menu[3].label == L"Launch at Login", "autostart keeps its plain label");
    Require(menu[4].label == L"Right-Click Speed Text for This Menu", "speed text toggle avoids internal jargon");

    const auto toggled = core.BuildOperatorMenu(nics, {false, true});
    Require(!toggled[3].checked && toggled[4].checked, "toggle marks follow persisted settings");
}

void FormatsBase1000BoundariesAndMinimumK()
{
    Require(WinMonCore::FormatRate(0.0) == L"0.0K/s", "zero format");
    Require(WinMonCore::FormatRate(999.0) == L"1.0K/s", "sub kilo format");
    Require(WinMonCore::FormatRate(1000.0) == L"1.0K/s", "K boundary");
    Require(WinMonCore::FormatRate(12345.0) == L"12.3K/s", "K rounding");
    Require(WinMonCore::FormatRate(1000000.0) == L"1.0M/s", "M boundary");
    Require(WinMonCore::FormatRate(999999999.0) == L"1000.0M/s", "M upper format");
    Require(WinMonCore::FormatRate(1000000000.0) == L"1.0G/s", "G boundary");
    Require(WinMonCore::FormatRate(-1.0).find(L"B") == std::wstring::npos, "no B unit");
    const std::locale previousLocale = std::locale();
    std::locale::global(std::locale(previousLocale, new CommaDecimalPunctuation));
    Require(WinMonCore::FormatRate(12345.0) == L"12.3K/s", "format uses product decimal point");
    std::locale::global(previousLocale);
}

}
int main()
{
    try
    {
        FirstSampleIsZero();
        SingleNicUsesActualElapsedTimeAndCounterDirections();
        AllNicsSumsOnlyUpNonLoopback();
        EmptyAndNoUpAreZero();
        BackwardCountersAndDisappearingNicsAreZero();
        NonpositiveElapsedIsZero();
        FormatsBase1000BoundariesAndMinimumK();
        RateStripVisibilityFollowsPrimaryBottomAvailability();
        MenuAndSelectionFollowEnumeration();
        OperatorMenuGroupsPersistedTogglesWithoutSeparator();
        ClassicConnectionsLimitNicSelectionChoices();
        SelectedNicOnlyAndChurnFallback();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "WinMonCore tests passed\n";
    return 0;
}
