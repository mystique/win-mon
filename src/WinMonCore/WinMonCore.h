#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace winmon
{
enum class OperatorMenuItemKind
{
    All,
    Nic,
    Separator,
    Autostart,
    RightClickSpeedText,
    CurrentFont,
    SetFont,
    Exit,
};

// Persisted operator settings that the Operator Menu renders as check marks.
struct OperatorMenuToggles final
{
    bool autostartEnabled = false;
    bool rightClickSpeedTextEnabled = false;
};

struct NicSnapshot final
{
    std::string stableId;
    std::wstring friendlyName;
    std::wstring description;
    std::wstring name;
    bool loopback = false;
    bool up = false;
    bool visibleInClassicConnections = true;
    std::uint64_t inOctets = 0;
    std::uint64_t outOctets = 0;
};

struct OperatorMenuItem final
{
    OperatorMenuItemKind kind = OperatorMenuItemKind::All;
    std::string stableId;
    std::wstring label;
    bool checked = false;
};

struct RateDisplay final
{
    double uploadBytesPerSecond = 0.0;
    double downloadBytesPerSecond = 0.0;
    std::wstring uploadText;
    std::wstring downloadText;
};

class WinMonCore final
{
public:
    RateDisplay Sample(const std::vector<NicSnapshot>& snapshots, double monotonicSeconds);
    std::vector<OperatorMenuItem> BuildOperatorMenu(
        const std::vector<NicSnapshot>& snapshots,
        const OperatorMenuToggles& toggles = {},
        const std::wstring& rateFontName = L"");
    void SelectAll() noexcept;
    void SelectNic(const std::string& stableId) noexcept;
    [[nodiscard]] bool IsAllSelected() const noexcept;
    [[nodiscard]] const std::string& SelectedNicId() const noexcept;
    [[nodiscard]] static bool ShouldShowRateStrip(bool primaryBottomTaskbarAvailable) noexcept;
    static std::wstring FormatRate(double bytesPerSecond);
    static std::wstring DisplayName(const NicSnapshot& snapshot);
private:
    struct PreviousSample final
    {
        std::string stableId;
        std::uint64_t inOctets = 0;
        std::uint64_t outOctets = 0;
    };
    std::vector<PreviousSample> previous_;
    double previousTime_ = 0.0;
    bool hasPrevious_ = false;
    void ReconcileSelection(const std::vector<NicSnapshot>& snapshots) noexcept;
    std::string selectedNicId_;
};
}
