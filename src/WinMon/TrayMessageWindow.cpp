#include "TrayMessageWindow.h"
#include "Autostart.h"
#include "Settings.h"

#include "res/resource.h"

#include <afxdlgs.h>
#include <shellapi.h>
#include <iphlpapi.h>
#include <netcon.h>
#include <netioapi.h>
#include <wrl/client.h>
#include <algorithm>
#include <chrono>
#include <vector>
#include <string>
#include <utility>
namespace
{
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayNotificationMessage = WM_APP + 1;
constexpr UINT kRightClickSpeedTextMessage = WM_APP + 2;
constexpr wchar_t kTrayTooltip[] = L"Win Mon";
constexpr UINT_PTR kRateSampleTimer = 1;
constexpr UINT_PTR kShellRecoveryTimer = 2;
constexpr UINT kShellRecoveryCadenceMs = 2000;
const UINT kTaskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
struct ClassicConnectionIds final
{
    bool available = false;
    std::vector<GUID> values;
};

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

bool AppendMenuItems(CMenu& menu, const std::vector<winmon::OperatorMenuItem>& items)
{
    for (const auto& item : items)
    {
        if (item.separator)
        {
            if (!menu.AppendMenu(MF_SEPARATOR)) return false;
            continue;
        }
        if (!item.children.empty())
        {
            CMenu submenu;
            if (!submenu.CreatePopupMenu() || !AppendMenuItems(submenu, item.children) ||
                !menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(submenu.GetSafeHmenu()), item.label.c_str()))
            {
                return false;
            }
            submenu.Detach();
            continue;
        }

        UINT flags = MF_STRING;
        if (item.checked) flags |= MF_CHECKED;
        if (item.radio) flags |= MFT_RADIOCHECK;
        if (!item.enabled) flags |= MF_DISABLED | MF_GRAYED;
        if (!menu.AppendMenu(flags, static_cast<UINT>(item.choiceToken), item.label.c_str())) return false;
    }
    return true;
}

struct OperatorMenuScope final
{
    explicit OperatorMenuScope(winmon::WinMonCore& core) noexcept : core(core) {}
    ~OperatorMenuScope() noexcept
    {
        if (!completed) core.CancelOperatorMenu();
    }
    OperatorMenuScope(const OperatorMenuScope&) = delete;
    OperatorMenuScope& operator=(const OperatorMenuScope&) = delete;

    winmon::WinMonCore& core;
    bool completed = false;
};
class RightClickSettingChange final : public winmon::OperatorSettingChange
{
public:
    RightClickSettingChange(RateStrip& rateStrip, bool enabled) noexcept
        : rateStrip_(rateStrip), previous_(rateStrip.IsContextMenuEnabled()), enabled_(enabled) {}

    bool ApplyLive() override
    {
        rateStrip_.SetContextMenuEnabled(enabled_);
        return true;
    }
    bool Persist() override { return settings::SetRightClickSpeedTextEnabled(enabled_); }
    bool RollbackLive() override
    {
        rateStrip_.SetContextMenuEnabled(previous_);
        return true;
    }

private:
    RateStrip& rateStrip_;
    bool previous_;
    bool enabled_;
};

class RateFontSettingChange final : public winmon::OperatorSettingChange
{
public:
    RateFontSettingChange(
        RateStrip& rateStrip,
        const RateFontSelection& previous,
        const RateFontSelection& selected) noexcept
        : rateStrip_(rateStrip), previous_(previous), selected_(selected) {}

    bool ApplyLive() override { return rateStrip_.SetRateFont(selected_); }
    bool Persist() override { return settings::SetRateFont(selected_); }
    bool RollbackLive() override { return rateStrip_.SetRateFont(previous_); }

private:
    RateStrip& rateStrip_;
    const RateFontSelection& previous_;
    const RateFontSelection& selected_;
};
}

BEGIN_MESSAGE_MAP(TrayMessageWindow, CWnd)
    ON_MESSAGE(kTrayNotificationMessage, &TrayMessageWindow::OnTrayNotification)
    ON_MESSAGE(kRightClickSpeedTextMessage, &TrayMessageWindow::OnRightClickSpeedText)
    ON_REGISTERED_MESSAGE(kTaskbarCreatedMessage, &TrayMessageWindow::OnTaskbarCreated)
    ON_MESSAGE(WM_DPICHANGED, &TrayMessageWindow::OnDpiChanged)
    ON_WM_DISPLAYCHANGE()
    ON_WM_SETTINGCHANGE()
    ON_WM_THEMECHANGED()

    ON_WM_TIMER()
END_MESSAGE_MAP()

TrayMessageWindow::TrayMessageWindow() noexcept : shellLifecycle_(*this) {}

bool TrayMessageWindow::Initialize()
{
    if (kTaskbarCreatedMessage == 0)
    {
        return false;
    }
    // TaskbarCreated is a system broadcast; message-only windows do not receive it.
    const CString windowClass = AfxRegisterWndClass(0);
    if (!CreateEx(
            0,
            windowClass,
            L"Win Mon Message Sink",
            WS_POPUP,
            0,
            0,
            0,
            0,
            nullptr,
            nullptr))
    {
        return false;
    }

    shuttingDown_ = false;
    rateStrip_.SetContextMenuOwner(GetSafeHwnd(), kRightClickSpeedTextMessage);
    rateStrip_.SetContextMenuEnabled(settings::IsRightClickSpeedTextEnabled());
    if (!shellLifecycle_.Start())
    {
        DestroyWindow();
        return false;
    }
    if (SetTimer(kRateSampleTimer, 1000, nullptr) == 0)
    {
        Shutdown();
        return false;
    }
    return true;
}

void TrayMessageWindow::Shutdown() noexcept
{
    shuttingDown_ = true;
    KillTimer(kRateSampleTimer);
    shellLifecycle_.Shutdown();

    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }
}



winmon::NetworkObservation TrayMessageWindow::ReadNetworkObservation() const
{
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || table == nullptr) return {};
    std::vector<winmon::NicSnapshot> snapshots;
    const auto classicConnectionIds = ReadClassicConnectionIds();
    const bool classicClassificationAvailable = classicConnectionIds.available;
    snapshots.reserve(table->NumEntries);
    for (ULONG index = 0; index < table->NumEntries; ++index)
    {
        const auto& row = table->Table[index];
        winmon::NicSnapshot snapshot;
        snapshot.stableId = std::to_string(row.InterfaceLuid.Value);
        snapshot.friendlyName = row.Alias;
        snapshot.description = row.Description;
        snapshot.loopback = row.Type == IF_TYPE_SOFTWARE_LOOPBACK;
        snapshot.visibleInClassicConnections =
            classicConnectionIds.available && ContainsConnectionId(classicConnectionIds.values, row.InterfaceGuid);
        snapshot.up = row.OperStatus == IfOperStatusUp;
        snapshot.inOctets = row.InOctets;
        snapshot.outOctets = row.OutOctets;
        snapshots.push_back(std::move(snapshot));
    }
    FreeMibTable(table);
    return {std::move(snapshots), classicClassificationAvailable};
}


LRESULT TrayMessageWindow::OnTaskbarCreated(WPARAM, LPARAM)
{
    shellLifecycle_.OnShellCreated();
    return 0;
}

LRESULT TrayMessageWindow::OnDpiChanged(WPARAM, LPARAM)
{
    shellLifecycle_.OnEnvironmentChanged();
    return 0;
}

void TrayMessageWindow::OnDisplayChange(UINT, int, int)
{
    shellLifecycle_.OnEnvironmentChanged();
}

void TrayMessageWindow::OnSettingChange(UINT, LPCTSTR)
{
    UpdateTrayIconTheme();
    shellLifecycle_.OnEnvironmentChanged();
}

LRESULT TrayMessageWindow::OnThemeChanged()
{
    UpdateTrayIconTheme();
    shellLifecycle_.OnEnvironmentChanged();
    return 0;
}

void TrayMessageWindow::SampleRates()
{
    core_.ObserveNetwork(ReadNetworkObservation());
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const double seconds = std::chrono::duration<double>(now).count();
    const auto display = core_.Sample(seconds);
    rateStrip_.SetRates(display.uploadText, display.downloadText);
}

void TrayMessageWindow::OnTimer(UINT_PTR timerId)
{
    if (timerId == kRateSampleTimer)
    {
        SampleRates();
        shellLifecycle_.OnRateSample();
    }
    else if (timerId == kShellRecoveryTimer)
    {
        CancelRecovery();
        shellLifecycle_.Retry();
    }
    CWnd::OnTimer(timerId);
}

HICON TrayMessageWindow::LoadTrayIcon() const noexcept
{
    DWORD value = 1;
    DWORD size = sizeof(value);
    const LSTATUS status = RegGetValueW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",
        RRF_RT_REG_DWORD,
        nullptr,
        &value,
        &size);
    const int resourceId = status == ERROR_SUCCESS && value != 0
        ? IDI_WINMON_TRAY_LIGHT
        : IDI_WINMON_TRAY_DARK;
    return static_cast<HICON>(::LoadImageW(
        AfxGetResourceHandle(),
        MAKEINTRESOURCEW(resourceId),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE | LR_SHARED));
}

void TrayMessageWindow::UpdateTrayIconTheme() noexcept
{
    if (!trayIconAdded_)
    {
        return;
    }
    NOTIFYICONDATAW iconData{};
    iconData.cbSize = sizeof(iconData);
    iconData.hWnd = GetSafeHwnd();
    iconData.uID = kTrayIconId;
    iconData.uFlags = NIF_ICON;
    iconData.hIcon = LoadTrayIcon();
    if (iconData.hIcon != nullptr)
    {
        Shell_NotifyIconW(NIM_MODIFY, &iconData);
    }
}

winmon::RateStripState TrayMessageWindow::RecreateRateStrip()
{
    rateStrip_.Shutdown();
    LoadSavedRateFont();
    if (!RateStrip::IsPrimaryBottomTaskbarAvailable())
    {
        return winmon::RateStripState::Hidden;
    }
    return rateStrip_.Embed(true) ? winmon::RateStripState::Visible : winmon::RateStripState::Retry;
}

winmon::RateStripState TrayMessageWindow::RefreshRateStrip()
{
    if (rateStrip_.GetSafeHwnd() != nullptr && rateStrip_.Refresh())
    {
        return winmon::RateStripState::Visible;
    }
    if (!RateStrip::IsPrimaryBottomTaskbarAvailable())
    {
        rateStrip_.Shutdown();
        return winmon::RateStripState::Hidden;
    }
    return winmon::RateStripState::Retry;
}

void TrayMessageWindow::DestroyRateStrip() noexcept
{
    rateStrip_.Shutdown();
}

void TrayMessageWindow::ForgetTrayIcon() noexcept
{
    trayIconAdded_ = false;
}

void TrayMessageWindow::ScheduleRecovery() noexcept
{
    if (shuttingDown_ || shellRecoveryTimerActive_ || GetSafeHwnd() == nullptr) return;
    shellRecoveryTimerActive_ = SetTimer(kShellRecoveryTimer, kShellRecoveryCadenceMs, nullptr) != 0;
}

void TrayMessageWindow::CancelRecovery() noexcept
{
    if (!shellRecoveryTimerActive_) return;
    KillTimer(kShellRecoveryTimer);
    shellRecoveryTimerActive_ = false;
}

void TrayMessageWindow::LoadSavedRateFont()
{
    RateFontSelection savedFont;
    if (settings::GetRateFont(savedFont))
    {
        static_cast<void>(rateStrip_.SetRateFont(savedFont));
    }
    else
    {
        static_cast<void>(rateStrip_.ResetRateFont());
    }
}

bool TrayMessageWindow::EnsureTrayIcon()
{
    if (trayIconAdded_) return true;
    NOTIFYICONDATAW iconData{};
    iconData.cbSize = sizeof(iconData);
    iconData.hWnd = GetSafeHwnd();
    iconData.uID = kTrayIconId;
    iconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    iconData.uCallbackMessage = kTrayNotificationMessage;
    iconData.hIcon = LoadTrayIcon();

    if (iconData.hIcon == nullptr)
    {
        return false;
    }

    wcscpy_s(iconData.szTip, kTrayTooltip);
    if (!Shell_NotifyIconW(NIM_ADD, &iconData))
    {
        // Explorer can still be replacing its notification area. Treat this
        // as transient during recovery; the bounded retry will try again.
        trayIconAdded_ = false;
        return false;
    }

    trayIconAdded_ = true;

    iconData.uVersion = NOTIFYICON_VERSION_4;
    Shell_NotifyIconW(NIM_SETVERSION, &iconData);
    return true;
}

void TrayMessageWindow::RemoveTrayIcon() noexcept
{
    if (!trayIconAdded_)
    {
        return;
    }

    NOTIFYICONDATAW iconData{};
    iconData.cbSize = sizeof(iconData);
    iconData.hWnd = GetSafeHwnd();
    iconData.uID = kTrayIconId;
    Shell_NotifyIconW(NIM_DELETE, &iconData);
    trayIconAdded_ = false;
}

winmon::OperatorAction TrayMessageWindow::ShowOperatorMenu()
{
    CPoint cursorPosition;
    if (!GetCursorPos(&cursorPosition)) return {};
    core_.ObserveNetwork(ReadNetworkObservation());
    const auto model = core_.BeginOperatorMenu(
        {autostart::IsEnabled(), rateStrip_.IsContextMenuEnabled()},
        rateStrip_.GetRateFontName());
    if (!model.has_value()) return {};
    OperatorMenuScope transaction{core_};

    CMenu menu;
    if (!menu.CreatePopupMenu() || !AppendMenuItems(menu, *model)) return {};
    SetForegroundWindow();
    const auto choiceToken = static_cast<std::uint32_t>(::TrackPopupMenu(
        menu.GetSafeHmenu(),
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        cursorPosition.x,
        cursorPosition.y,
        0,
        GetSafeHwnd(),
        nullptr));
    PostMessage(WM_NULL);
    transaction.completed = true;
    return core_.CompleteOperatorMenu(choiceToken);
}

void TrayMessageWindow::RequestExit()
{
    Shutdown();
    PostQuitMessage(0);
}
void TrayMessageWindow::ChooseRateFont()
{
    RateFontSelection selection;
    if (!rateStrip_.GetRateFont(selection))
    {
        return;
    }
    const RateFontSelection previousSelection = selection;

    CFontDialog dialog(
        &selection.logFont,
        CF_SCREENFONTS | CF_INITTOLOGFONTSTRUCT | CF_FORCEFONTEXIST |
            CF_NOVERTFONTS | CF_NOSCRIPTSEL | CF_LIMITSIZE,
        nullptr,
        this);
    dialog.m_cf.nSizeMin = 6;
    dialog.m_cf.nSizeMax = 12;
    dialog.m_cf.iPointSize = selection.pointSizeTenths;
    if (dialog.DoModal() != IDOK)
    {
        return;
    }

    dialog.GetCurrentFont(&selection.logFont);
    selection.pointSizeTenths = dialog.GetSize();
    RateFontSettingChange change{rateStrip_, previousSelection, selection};
    if (winmon::OperatorSettingTransaction::Commit(change) == winmon::OperatorSettingOutcome::RecoveryRequired)
    {
        // The persisted value is still the previous choice. Recreate through
        // the lifecycle so recovery keeps retrying until that choice is live.
        shellLifecycle_.OnEnvironmentChanged();
    }
}

void TrayMessageWindow::HandleOperatorMenuAction(winmon::OperatorAction action)
{
    switch (action.kind)
    {
    case winmon::OperatorActionKind::EnableLaunchAtLogin:
        static_cast<void>(autostart::Enable());
        break;
    case winmon::OperatorActionKind::DisableLaunchAtLogin:
        static_cast<void>(autostart::Disable());
        break;
    case winmon::OperatorActionKind::EnableRightClickSpeedText:
    case winmon::OperatorActionKind::DisableRightClickSpeedText:
    {
        const bool enabled = action.kind == winmon::OperatorActionKind::EnableRightClickSpeedText;
        RightClickSettingChange change{rateStrip_, enabled};
        static_cast<void>(winmon::OperatorSettingTransaction::Commit(change));
        break;
    }
    case winmon::OperatorActionKind::SetRateFont:
        ChooseRateFont();
        break;
    case winmon::OperatorActionKind::Exit:
        RequestExit();
        break;
    case winmon::OperatorActionKind::None:
        break;
    }
}

LRESULT TrayMessageWindow::OnTrayNotification(WPARAM, LPARAM lParam)
{
    const UINT notification = LOWORD(lParam);
    if (notification != WM_RBUTTONUP && notification != WM_CONTEXTMENU) return 0;
    HandleOperatorMenuAction(ShowOperatorMenu());
    return 0;
}

LRESULT TrayMessageWindow::OnRightClickSpeedText(WPARAM, LPARAM)
{
    if (shuttingDown_) return 0;
    HandleOperatorMenuAction(ShowOperatorMenu());
    return 0;
}
