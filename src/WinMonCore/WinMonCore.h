#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace winmon
{
enum class RateStripState
{
    Visible,
    Hidden,
    Retry,
};

class ShellSurface
{
public:
    virtual ~ShellSurface() = default;
    [[nodiscard]] virtual bool EnsureTrayIcon() = 0;
    virtual void ForgetTrayIcon() noexcept = 0;
    [[nodiscard]] virtual RateStripState RecreateRateStrip() = 0;
    [[nodiscard]] virtual RateStripState RefreshRateStrip() = 0;
    virtual void DestroyRateStrip() noexcept = 0;
    virtual void RemoveTrayIcon() noexcept = 0;
    virtual void ScheduleRecovery() noexcept = 0;
    virtual void CancelRecovery() noexcept = 0;
};

class ShellLifecycle final
{
public:
    explicit ShellLifecycle(ShellSurface& surface) noexcept;
    [[nodiscard]] bool Start();
    void OnShellCreated();
    void OnEnvironmentChanged();
    void OnRateSample();
    void Retry();
    void Shutdown() noexcept;

private:
    void RestoreShellSurface();
    void HandleRateStripState(RateStripState state) noexcept;

    ShellSurface& surface_;
    bool running_ = false;
};

enum class OperatorActionKind
{
    None,
    EnableLaunchAtLogin,
    DisableLaunchAtLogin,
    EnableRightClickSpeedText,
    DisableRightClickSpeedText,
    SetRateFont,
    Exit,
};

struct OperatorAction final
{
    OperatorActionKind kind = OperatorActionKind::None;
};
enum class OperatorSettingOutcome
{
    Applied,
    LiveApplicationFailed,
    RolledBack,
    RecoveryRequired,
};

class OperatorSettingChange
{
public:
    virtual ~OperatorSettingChange() = default;
    [[nodiscard]] virtual bool ApplyLive() = 0;
    [[nodiscard]] virtual bool Persist() = 0;
    [[nodiscard]] virtual bool RollbackLive() = 0;
};

class OperatorSettingTransaction final
{
public:
    [[nodiscard]] static OperatorSettingOutcome Commit(OperatorSettingChange& change);
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
struct NetworkObservation final
{
    std::vector<NicSnapshot> snapshots;
    bool classicClassificationAvailable = false;
};


struct OperatorMenuItem final
{
    std::wstring label;
    std::uint32_t choiceToken = 0;
    bool checked = false;
    bool radio = false;
    bool enabled = true;
    bool separator = false;
    std::vector<OperatorMenuItem> children;
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
    void ObserveNetwork(NetworkObservation observation);
    RateDisplay Sample(double monotonicSeconds);
    std::optional<std::vector<OperatorMenuItem>> BeginOperatorMenu(
        const OperatorMenuToggles& toggles = {},
        const std::wstring& rateFontName = L"");
    OperatorAction CompleteOperatorMenu(std::uint32_t choiceToken) noexcept;
    void CancelOperatorMenu() noexcept;
    static std::wstring FormatRate(double bytesPerSecond);

private:
    struct PreviousSample final
    {
        std::string stableId;
        std::uint64_t inOctets = 0;
        std::uint64_t outOctets = 0;
    };
    RateDisplay SampleObserved(const std::vector<NicSnapshot>& snapshots, double monotonicSeconds);
    std::vector<OperatorMenuItem> BuildOperatorMenu(
        const std::vector<NicSnapshot>& snapshots,
        const OperatorMenuToggles& toggles,
        const std::wstring& rateFontName);
    void SelectAll() noexcept;
    void SelectNic(const std::string& stableId) noexcept;
    static std::wstring DisplayName(const NicSnapshot& snapshot);
    struct PendingMenuChoice final
    {
        OperatorAction action;
        std::string selectedNicId;
        bool selectAll = false;
    };
    void AddChoice(OperatorMenuItem& item, PendingMenuChoice choice);
    std::vector<PreviousSample> previous_;
    double previousTime_ = 0.0;
    bool hasPrevious_ = false;
    void ReconcileSelection(const std::vector<NicSnapshot>& snapshots) noexcept;
    std::vector<NicSnapshot> snapshots_;
    std::vector<PendingMenuChoice> pendingMenuChoices_;
    bool classicClassificationAvailable_ = false;
    bool menuOpen_ = false;
    std::string selectedNicId_;
};
}
