#include "WinMonCore.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace winmon
{
ShellLifecycle::ShellLifecycle(ShellSurface& surface) noexcept : surface_(surface) {}

bool ShellLifecycle::Start()
{
    if (!surface_.EnsureTrayIcon())
    {
        return false;
    }
    running_ = true;
    HandleRateStripState(surface_.RecreateRateStrip());
    return true;
}

void ShellLifecycle::OnShellCreated()
{
    if (!running_) return;
    surface_.ForgetTrayIcon();
    RestoreShellSurface();
}

void ShellLifecycle::OnEnvironmentChanged()
{
    if (!running_) return;
    HandleRateStripState(surface_.RecreateRateStrip());
}

void ShellLifecycle::OnRateSample()
{
    if (!running_) return;
    HandleRateStripState(surface_.RefreshRateStrip());
}

void ShellLifecycle::Retry()
{
    if (!running_) return;
    RestoreShellSurface();
}

void ShellLifecycle::Shutdown() noexcept
{
    if (!running_) return;
    running_ = false;
    surface_.CancelRecovery();
    surface_.DestroyRateStrip();
    surface_.RemoveTrayIcon();
}

void ShellLifecycle::RestoreShellSurface()
{
    if (!surface_.EnsureTrayIcon())
    {
        surface_.ScheduleRecovery();
        return;
    }
    HandleRateStripState(surface_.RecreateRateStrip());
}

void ShellLifecycle::HandleRateStripState(RateStripState state) noexcept
{
    if (state == RateStripState::Retry)
    {
        surface_.ScheduleRecovery();
    }
    else
    {
        surface_.CancelRecovery();
    }
}



OperatorSettingOutcome OperatorSettingTransaction::Commit(OperatorSettingChange& change)
{
    if (!change.ApplyLive()) return OperatorSettingOutcome::LiveApplicationFailed;
    if (change.Persist()) return OperatorSettingOutcome::Applied;
    return change.RollbackLive()
        ? OperatorSettingOutcome::RolledBack
        : OperatorSettingOutcome::RecoveryRequired;
}

void WinMonCore::ObserveNetwork(NetworkObservation observation)
{
    snapshots_ = std::move(observation.snapshots);
    classicClassificationAvailable_ = observation.classicClassificationAvailable;
    sampledAt_ = observation.sampledAt;
    ReconcileSelection(snapshots_);
}

RateDisplay WinMonCore::Sample()
{
    return SampleObserved(snapshots_, sampledAt_);
}

std::optional<std::vector<OperatorMenuItem>> WinMonCore::BeginOperatorMenu(
    const OperatorMenuToggles& toggles,
    const std::wstring& rateFontName)
{
    if (menuOpen_) return std::nullopt;
    menuOpen_ = true;
    pendingMenuChoices_.clear();

    auto menuSnapshots = snapshots_;
    if (!classicClassificationAvailable_)
    {
        for (auto& snapshot : menuSnapshots) snapshot.visibleInClassicConnections = true;
    }
    return BuildOperatorMenu(menuSnapshots, toggles, rateFontName);
}

OperatorAction WinMonCore::CompleteOperatorMenu(std::uint32_t choiceToken) noexcept
{
    if (!menuOpen_) return {};
    menuOpen_ = false;
    if (choiceToken == 0 || choiceToken > pendingMenuChoices_.size())
    {
        pendingMenuChoices_.clear();
        return {};
    }

    const PendingMenuChoice choice = pendingMenuChoices_[choiceToken - 1];
    pendingMenuChoices_.clear();
    if (choice.selectAll)
    {
        SelectAll();
    }
    else if (!choice.selectedNicId.empty())
    {
        SelectNic(choice.selectedNicId);
    }
    return choice.action;
}

void WinMonCore::CancelOperatorMenu() noexcept
{
    menuOpen_ = false;
    pendingMenuChoices_.clear();
}

void WinMonCore::AddChoice(OperatorMenuItem& item, PendingMenuChoice choice)
{
    pendingMenuChoices_.push_back(std::move(choice));
    item.choiceToken = static_cast<std::uint32_t>(pendingMenuChoices_.size());
}

RateDisplay WinMonCore::SampleObserved(
    const std::vector<NicSnapshot>& snapshots,
    std::chrono::steady_clock::time_point sampledAt)
{
    ReconcileSelection(snapshots);
    RateDisplay display;
    display.networkGeneration = networkGeneration_;
    const double elapsed = std::chrono::duration<double>(sampledAt - previousTime_).count();
    const bool validElapsed = hasPrevious_ && elapsed > 0.0;
    if (validElapsed)
    {
        for (const auto& current : snapshots)
        {
            const bool excludedFromAll = selectedNicId_.empty() &&
                (!current.hardwareInterface ||
                 (classicClassificationAvailable_ && !current.visibleInClassicConnections));
            if (!current.up || current.loopback || excludedFromAll ||
                (!selectedNicId_.empty() && current.stableId != selectedNicId_)) continue;
            const auto previous = std::find_if(previous_.begin(), previous_.end(), [&current](const PreviousSample& sample) { return sample.stableId == current.stableId; });
            if (previous == previous_.end()) continue;
            if (current.inOctets >= previous->inOctets)
            {
                display.downloadBytesPerSecond += static_cast<double>(current.inOctets - previous->inOctets) / elapsed;
            }
            if (current.outOctets >= previous->outOctets)
            {
                display.uploadBytesPerSecond += static_cast<double>(current.outOctets - previous->outOctets) / elapsed;
            }
        }
    }
    previous_.clear();
    previous_.reserve(snapshots.size());
    for (const auto& snapshot : snapshots) previous_.push_back({snapshot.stableId, snapshot.inOctets, snapshot.outOctets});
    previousTime_ = sampledAt;
    hasPrevious_ = true;
    display.uploadText = FormatRate(display.uploadBytesPerSecond);
    display.downloadText = FormatRate(display.downloadBytesPerSecond);
    return display;
}

std::wstring WinMonCore::DisplayName(const NicSnapshot& snapshot)
{
    if (!snapshot.friendlyName.empty()) return snapshot.friendlyName;
    if (!snapshot.name.empty()) return snapshot.name;
    return snapshot.description;
}

std::vector<OperatorMenuItem> WinMonCore::BuildOperatorMenu(
    const std::vector<NicSnapshot>& snapshots,
    const OperatorMenuToggles& toggles,
    const std::wstring& rateFontName)
{
    ReconcileSelection(snapshots);
    std::vector<OperatorMenuItem> menu;

    OperatorMenuItem network;
    network.label = L"Network to Monitor";
    OperatorMenuItem all;
    all.label = L"All";
    all.checked = selectedNicId_.empty();
    all.radio = true;
    AddChoice(all, {{}, {}, true});
    network.children.push_back(std::move(all));
    for (const auto& snapshot : snapshots)
    {
        if (snapshot.loopback || !snapshot.visibleInClassicConnections) continue;
        OperatorMenuItem nic;
        nic.label = DisplayName(snapshot);
        nic.checked = snapshot.stableId == selectedNicId_;
        nic.radio = true;
        AddChoice(nic, {{}, snapshot.stableId, false});
        network.children.push_back(std::move(nic));
    }
    menu.push_back(std::move(network));
    menu.push_back({.separator = true});

    OperatorMenuItem autostart;
    autostart.label = L"Launch at Login";
    autostart.checked = toggles.autostartEnabled;
    AddChoice(autostart, {{toggles.autostartEnabled
        ? OperatorActionKind::DisableLaunchAtLogin
        : OperatorActionKind::EnableLaunchAtLogin}});
    menu.push_back(std::move(autostart));

    OperatorMenuItem rightClick;
    rightClick.label = L"Right-Click Speed Text";
    rightClick.checked = toggles.rightClickSpeedTextEnabled;
    AddChoice(rightClick, {{toggles.rightClickSpeedTextEnabled
        ? OperatorActionKind::DisableRightClickSpeedText
        : OperatorActionKind::EnableRightClickSpeedText}});
    menu.push_back(std::move(rightClick));

    OperatorMenuItem floatingDisplay;
    floatingDisplay.label = L"Show Floating Display";
    floatingDisplay.checked = toggles.floatingRateDisplayEnabled;
    AddChoice(floatingDisplay, {{toggles.floatingRateDisplayEnabled
        ? OperatorActionKind::DisableFloatingRateDisplay
        : OperatorActionKind::EnableFloatingRateDisplay}});
    menu.push_back(std::move(floatingDisplay));
    menu.push_back({.separator = true});

    menu.push_back({L"Font: " + rateFontName, 0, false, false, false});
    OperatorMenuItem setFont;
    setFont.label = L"Set Font...";
    AddChoice(setFont, {{OperatorActionKind::SetRateFont}});
    menu.push_back(std::move(setFont));
    menu.push_back({.separator = true});

    OperatorMenuItem exit;
    exit.label = L"Exit";
    AddChoice(exit, {{OperatorActionKind::Exit}});
    menu.push_back(std::move(exit));
    return menu;
}

void WinMonCore::SelectAll() noexcept { SelectNic({}); }
void WinMonCore::SelectNic(const std::string& stableId) noexcept
{
    if (selectedNicId_ != stableId)
    {
        selectedNicId_ = stableId;
        ++networkGeneration_;
    }
}

void WinMonCore::ReconcileSelection(const std::vector<NicSnapshot>& snapshots) noexcept
{
    if (selectedNicId_.empty())
    {
        return;
    }

    const auto selected = std::find_if(
        snapshots.begin(),
        snapshots.end(),
        [this](const NicSnapshot& snapshot) { return !snapshot.loopback && snapshot.stableId == selectedNicId_; });
    if (selected == snapshots.end())
    {
        SelectAll();
    }
}


std::wstring WinMonCore::FormatRate(double bytesPerSecond)
{
    const double magnitude = std::max(0.0, bytesPerSecond);
    const wchar_t* suffix = L"K/s";
    double scaled = magnitude / 1000.0;
    if (magnitude >= 1'000'000'000.0)
    {
        suffix = L"G/s";
        scaled = magnitude / 1'000'000'000.0;
    }
    else if (magnitude >= 1'000'000.0)
    {
        suffix = L"M/s";
        scaled = magnitude / 1'000'000.0;
    }

    std::wostringstream text;
    text.imbue(std::locale::classic());
    text << std::fixed << std::setprecision(1) << scaled << L' ' << suffix;
    return text.str();
}
}
