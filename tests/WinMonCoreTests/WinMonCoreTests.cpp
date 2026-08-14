#include "WinMonCore.h"
#include <algorithm>

#include <chrono>
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
using winmon::RateStripState;
using winmon::ShellLifecycle;
using winmon::ShellSurface;
using winmon::OperatorActionKind;
using winmon::OperatorMenuItem;
using winmon::OperatorSettingChange;
using winmon::OperatorSettingOutcome;
using winmon::OperatorSettingTransaction;

class CommaDecimalPunctuation final : public std::numpunct<wchar_t>
{
protected:
    wchar_t do_decimal_point() const override { return L','; }
};

NicSnapshot Nic(
    const char* id,
    bool up,
    std::uint64_t inOctets,
    std::uint64_t outOctets,
    bool loopback = false,
    bool hardwareInterface = true)
{
    NicSnapshot nic;
    nic.stableId = id;
    nic.friendlyName = L"";
    nic.up = up;
    nic.loopback = loopback;
    nic.hardwareInterface = hardwareInterface;
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

std::chrono::steady_clock::time_point RateSampleAt(double seconds)
{
    return std::chrono::steady_clock::time_point{
        std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(seconds))};
}


void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}
void RequireRate(const winmon::RateDisplay& display, double upload, double download);

class FakeShellSurface final : public ShellSurface
{
public:
    bool EnsureTrayIcon() override
    {
        ++ensureTrayCalls;
        return trayResult;
    }
    void ForgetTrayIcon() noexcept override { ++forgetTrayCalls; }
    RateStripState RecreateRateStrip() override
    {
        ++recreateCalls;
        return recreateResult;
    }
    RateStripState RefreshRateStrip() override
    {
        ++refreshCalls;
        return refreshResult;
    }
    void DestroyRateStrip() noexcept override { ++destroyCalls; }
    void RemoveTrayIcon() noexcept override { ++removeTrayCalls; }
    void ScheduleRecovery() noexcept override { recoveryScheduled = true; }
    void CancelRecovery() noexcept override { recoveryScheduled = false; }

    bool trayResult = true;
    RateStripState recreateResult = RateStripState::Visible;
    RateStripState refreshResult = RateStripState::Visible;
    int ensureTrayCalls = 0;
    int forgetTrayCalls = 0;
    int recreateCalls = 0;
    int refreshCalls = 0;
    int destroyCalls = 0;
    int removeTrayCalls = 0;
    bool recoveryScheduled = false;
};

void ShellLifecycleOwnsRecoveryAndVisibility()
{
    FakeShellSurface surface;
    ShellLifecycle lifecycle(surface);
    Require(lifecycle.Start(), "supported shell starts");
    Require(surface.ensureTrayCalls == 1 && surface.recreateCalls == 1, "startup creates both shell surfaces");

    surface.refreshResult = RateStripState::Retry;
    lifecycle.OnRateSample();
    Require(surface.refreshCalls == 1 && surface.recoveryScheduled, "transient refresh loss schedules recovery");

    surface.recreateResult = RateStripState::Hidden;
    lifecycle.Retry();
    Require(!surface.recoveryScheduled, "unsupported taskbar is hidden without retry");

    surface.recreateResult = RateStripState::Visible;
    lifecycle.OnShellCreated();
    Require(surface.forgetTrayCalls == 1 && surface.ensureTrayCalls == 3, "shell recreation forgets stale tray ownership");
    Require(surface.recreateCalls == 3 && !surface.recoveryScheduled, "shell recreation restores one Rate Strip");

    lifecycle.OnEnvironmentChanged();
    Require(surface.recreateCalls == 4, "DPI display setting and theme signals share recreation");

    lifecycle.Shutdown();
    Require(surface.destroyCalls == 1 && surface.removeTrayCalls == 1 && !surface.recoveryScheduled, "shutdown removes chrome and recovery");
    lifecycle.Retry();
    Require(surface.ensureTrayCalls == 3, "shutdown ignores pending recovery");
}

void ShellLifecycleTreatsInitialTrayFailureAsFatal()
{
    FakeShellSurface surface;
    surface.trayResult = false;
    ShellLifecycle lifecycle(surface);
    Require(!lifecycle.Start(), "initial tray icon failure is fatal");
    Require(surface.recreateCalls == 0 && !surface.recoveryScheduled, "failed startup does not begin background recovery");
}

const OperatorMenuItem* FindItemPointer(const std::vector<OperatorMenuItem>& items, const wchar_t* label)
{
    for (const auto& item : items)
    {
        if (item.label == label) return &item;
        if (const auto* nested = FindItemPointer(item.children, label)) return nested;
    }
    return nullptr;
}

const OperatorMenuItem& FindItem(const std::vector<OperatorMenuItem>& items, const wchar_t* label)
{
    const auto* item = FindItemPointer(items, label);
    Require(item != nullptr, "expected Operator Menu item");
    return *item;
}

void NetworkSessionPresentsOneCanonicalObservation()
{
    WinMonCore core;
    auto hidden = NamedNic("internal", L"Internal", L"Internal transport");
    hidden.visibleInClassicConnections = false;
    core.ObserveNetwork({{NamedNic("up", L"Ethernet", L"Intel"), NamedNic("down", L"", L"Disconnected", false), hidden}, true, RateSampleAt(1.0)});
    core.Sample();

    const auto opened = core.BeginOperatorMenu({true, false}, L"Cascadia Mono");
    Require(opened.has_value(), "first Operator Menu transaction opens");
    const auto& down = FindItem(*opened, L"Disconnected");
    Require(FindItem(*opened, L"All").checked, "all is selected on launch");
    Require(FindItemPointer(*opened, L"Internal") == nullptr, "classic classification filters menu choices");
    Require(!core.BeginOperatorMenu({}, L"").has_value(), "nested Operator Menu is rejected");
    Require(core.CompleteOperatorMenu(down.choiceToken).kind == OperatorActionKind::None, "network choice is completed inside transaction");

    auto observedUp = NamedNic("up", L"Ethernet", L"Intel");
    observedUp.inOctets = 1000;
    observedUp.outOctets = 1000;
    auto observedDown = NamedNic("down", L"", L"Disconnected", false);
    observedDown.inOctets = 2000;
    observedDown.outOctets = 3000;
    core.ObserveNetwork({{observedUp, observedDown}, true, RateSampleAt(2.0)});
    RequireRate(core.Sample(), 0.0, 0.0);
    const auto downMenu = core.BeginOperatorMenu({}, L"Consolas");
    Require(FindItem(*downMenu, L"Disconnected").checked, "available down NIC remains selected");
    core.CancelOperatorMenu();

    observedUp.inOctets = 2000;
    observedUp.outOctets = 4000;
    core.ObserveNetwork({{observedUp}, true, RateSampleAt(3.0)});
    RequireRate(core.Sample(), 3000.0, 1000.0);
    const auto fallbackMenu = core.BeginOperatorMenu({}, L"Consolas");
    Require(FindItem(*fallbackMenu, L"All").checked, "disappearance reconciles to all before rates and menu");
    core.CancelOperatorMenu();
}

void ClassificationFallbackPreservesAllAndSelectedNicRates()
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

    const auto menu = core.BeginOperatorMenu({}, L"Segoe UI");
    const auto& internal = FindItem(*menu, L"Internal");
    Require(core.CompleteOperatorMenu(internal.choiceToken).kind == OperatorActionKind::None, "network choice is applied");

    otherwiseHidden.inOctets = 4000;
    otherwiseHidden.outOctets = 6000;
    core.ObserveNetwork({{otherwiseHidden}, true, RateSampleAt(3.0)});
    RequireRate(core.Sample(), 4000.0, 3000.0);
}

void OperatorMenuTransactionCorrelatesEveryAction()
{
    WinMonCore core;
    core.ObserveNetwork({{NamedNic("a", L"Alpha", L"A desc")}, true});
    auto menu = core.BeginOperatorMenu({false, true}, L"Cascadia Mono");
    Require(!FindItem(*menu, L"Launch at Login").checked, "Launch at Login reflects live state");
    Require(FindItem(*menu, L"Right-Click Speed Text").checked, "Right-Click Speed Text reflects live state");
    FindItem(*menu, L"Font: Cascadia Mono");
    Require(std::count_if(menu->begin(), menu->end(), [](const OperatorMenuItem& item) {
        return item.separator;
    }) == 3, "Operator Menu preserves network toggle font and Exit groups");
    const auto& autostart = FindItem(*menu, L"Launch at Login");
    Require(core.CompleteOperatorMenu(autostart.choiceToken).kind == OperatorActionKind::EnableLaunchAtLogin, "autostart choice meaning");

    menu = core.BeginOperatorMenu({true, true}, L"Cascadia Mono");
    const auto& rightClick = FindItem(*menu, L"Right-Click Speed Text");
    Require(core.CompleteOperatorMenu(rightClick.choiceToken).kind == OperatorActionKind::DisableRightClickSpeedText, "right-click choice meaning");

    menu = core.BeginOperatorMenu({}, L"Cascadia Mono");
    const auto& setFont = FindItem(*menu, L"Set Font...");
    Require(core.CompleteOperatorMenu(setFont.choiceToken).kind == OperatorActionKind::SetRateFont, "font choice meaning");

    menu = core.BeginOperatorMenu({}, L"Cascadia Mono");
    const auto& exit = FindItem(*menu, L"Exit");
    Require(core.CompleteOperatorMenu(exit.choiceToken).kind == OperatorActionKind::Exit, "exit choice meaning");
}
class FakeOperatorSettingChange final : public OperatorSettingChange
{
public:
    bool ApplyLive() override
    {
        ++applyCalls;
        return applyResult;
    }
    bool Persist() override
    {
        ++persistCalls;
        return persistResult;
    }
    bool RollbackLive() override
    {
        ++rollbackCalls;
        return rollbackResult;
    }

    bool applyResult = true;
    bool persistResult = true;
    bool rollbackResult = true;
    int applyCalls = 0;
    int persistCalls = 0;
    int rollbackCalls = 0;
};

void OperatorSettingTransactionPreservesPreviousChoice()
{
    FakeOperatorSettingChange change;
    change.applyResult = false;
    Require(
        OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::LiveApplicationFailed,
        "live failure rejects setting before persistence");
    Require(change.persistCalls == 0 && change.rollbackCalls == 0, "live failure leaves storage untouched");

    change = {};
    change.persistResult = false;
    Require(
        OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::RolledBack,
        "storage failure restores previous live setting");
    Require(change.applyCalls == 1 && change.persistCalls == 1 && change.rollbackCalls == 1, "rollback transaction order");

    change = {};
    change.persistResult = false;
    change.rollbackResult = false;
    Require(
        OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::RecoveryRequired,
        "failed rollback requests recreation from persisted truth");

    change = {};
    Require(
        OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::Applied,
        "successful change applies and persists exactly once");
    Require(change.rollbackCalls == 0, "successful change needs no rollback");
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
    core.ObserveNetwork({{Nic("a", true, 100, 200)}, false, RateSampleAt(10.0)});
    const auto display = core.Sample();
    RequireRate(display, 0.0, 0.0);
    Require(display.uploadText == L"0.0K/s" && display.downloadText == L"0.0K/s", "first sample formatting");
}

void RateSampleUsesCounterCaptureTimes()
{
    WinMonCore core;
    core.ObserveNetwork({{Nic("a", true, 1000, 2000)}, false, RateSampleAt(3.0)});
    core.Sample();
    core.ObserveNetwork({{Nic("a", true, 5000, 5000)}, false, RateSampleAt(5.0)});
    const auto display = core.Sample();
    RequireRate(display, 1500.0, 2000.0);
}

void AllNicsSumsOnlyVisibleUpHardwareNonLoopback()
{
    WinMonCore core;
    auto hidden = Nic("hidden", true, 0, 0);
    hidden.visibleInClassicConnections = false;
    core.ObserveNetwork({{
        Nic("physical-a", true, 0, 0),
        Nic("physical-b", true, 0, 0),
        Nic("virtual", true, 0, 0, false, false),
        Nic("down", false, 0, 0),
        Nic("loop", true, 0, 0, true),
        hidden}, true, RateSampleAt(1.0)});
    core.Sample();

    hidden.inOctets = 500000;
    hidden.outOctets = 600000;
    core.ObserveNetwork({{
        Nic("physical-a", true, 1000, 2000),
        Nic("physical-b", true, 3000, 7000),
        Nic("virtual", true, 700000, 800000, false, false),
        Nic("down", false, 100000, 100000),
        Nic("loop", true, 900000, 900000, true),
        hidden}, true, RateSampleAt(2.0)});
    const auto display = core.Sample();
    RequireRate(display, 9000.0, 4000.0);
}

void SelectedVirtualNicStillReportsRates()
{
    WinMonCore core;
    auto virtualNic = NamedNic("virtual", L"Meta", L"Meta Tunnel");
    virtualNic.hardwareInterface = false;
    core.ObserveNetwork({{virtualNic}, true, RateSampleAt(1.0)});
    RequireRate(core.Sample(), 0.0, 0.0);

    const auto menu = core.BeginOperatorMenu();
    const auto& meta = FindItem(*menu, L"Meta");
    core.CompleteOperatorMenu(meta.choiceToken);

    virtualNic.inOctets = 3000;
    virtualNic.outOctets = 5000;
    core.ObserveNetwork({{virtualNic}, true, RateSampleAt(2.0)});
    RequireRate(core.Sample(), 5000.0, 3000.0);
}

void EmptyAndNoUpAreZero()
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

void BackwardCountersAndDisappearingNicsAreZero()
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




void NonpositiveElapsedIsZero()
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
        ShellLifecycleOwnsRecoveryAndVisibility();
        ShellLifecycleTreatsInitialTrayFailureAsFatal();
        NetworkSessionPresentsOneCanonicalObservation();
        ClassificationFallbackPreservesAllAndSelectedNicRates();
        OperatorMenuTransactionCorrelatesEveryAction();
        OperatorSettingTransactionPreservesPreviousChoice();
        FirstSampleIsZero();
        RateSampleUsesCounterCaptureTimes();
        AllNicsSumsOnlyVisibleUpHardwareNonLoopback();
        SelectedVirtualNicStillReportsRates();
        EmptyAndNoUpAreZero();
        BackwardCountersAndDisappearingNicsAreZero();
        NonpositiveElapsedIsZero();
        FormatsBase1000BoundariesAndMinimumK();
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
    std::cout << "WinMonCore tests passed\n";
    return 0;
}
