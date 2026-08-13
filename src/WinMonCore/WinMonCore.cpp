#include "WinMonCore.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <locale>
#include <sstream>

namespace winmon
{

RateDisplay WinMonCore::Sample(const std::vector<NicSnapshot>& snapshots, double monotonicSeconds)
{
    ReconcileSelection(snapshots);
    RateDisplay display;
    const double elapsed = monotonicSeconds - previousTime_;
    const bool validElapsed = hasPrevious_ && elapsed > 0.0;
    if (validElapsed)
    {
        for (const auto& current : snapshots)
        {
            if (!current.up || current.loopback || (!selectedNicId_.empty() && current.stableId != selectedNicId_)) continue;
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
    previousTime_ = monotonicSeconds;
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
    const OperatorMenuToggles& toggles)
{
    ReconcileSelection(snapshots);
    std::vector<OperatorMenuItem> menu;
    menu.push_back({OperatorMenuItemKind::All, {}, L"All", selectedNicId_.empty()});
    for (const auto& snapshot : snapshots)
    {
        if (snapshot.loopback || !snapshot.visibleInClassicConnections) continue;
        menu.push_back({OperatorMenuItemKind::Nic, snapshot.stableId, DisplayName(snapshot), snapshot.stableId == selectedNicId_});
    }
    menu.push_back({OperatorMenuItemKind::Separator, {}, {}, false});
    menu.push_back({OperatorMenuItemKind::Autostart, {}, L"Launch at Login", toggles.autostartEnabled});
    menu.push_back({OperatorMenuItemKind::RateStripContextMenu, {}, L"Right-Click Speed Text", toggles.rateStripContextMenuEnabled});
    menu.push_back({OperatorMenuItemKind::Separator, {}, {}, false});
    menu.push_back({OperatorMenuItemKind::Exit, {}, L"Exit", false});
    return menu;
}

void WinMonCore::SelectAll() noexcept { selectedNicId_.clear(); }
void WinMonCore::SelectNic(const std::string& stableId) noexcept { selectedNicId_ = stableId; }
bool WinMonCore::IsAllSelected() const noexcept { return selectedNicId_.empty(); }
const std::string& WinMonCore::SelectedNicId() const noexcept { return selectedNicId_; }

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
        selectedNicId_.clear();
    }
}

bool WinMonCore::ShouldShowRateStrip(bool primaryBottomTaskbarAvailable) noexcept
{
    return primaryBottomTaskbarAvailable;
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
    text << std::fixed << std::setprecision(1) << scaled << suffix;
    return text.str();
}
}
