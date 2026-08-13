#include "TrayMessageWindow.h"
#include "Autostart.h"
#include "Settings.h"

#include "res/resource.h"

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

bool AppendToggle(CMenu& menu, UINT command, const winmon::OperatorMenuItem& item)
{
    const UINT flags = MF_STRING | (item.checked ? MF_CHECKED : MF_UNCHECKED);
    return menu.AppendMenu(flags, command, item.label.c_str()) != FALSE;
}

// Marks the Operator Menu as open for as long as it is being built and tracked.
struct MenuOpenScope final
{
    explicit MenuOpenScope(bool& flag) noexcept : flag(flag) { flag = true; }
    ~MenuOpenScope() noexcept { flag = false; }
    MenuOpenScope(const MenuOpenScope&) = delete;
    MenuOpenScope& operator=(const MenuOpenScope&) = delete;

    bool& flag;
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

    if (!AddTrayIcon())
    {
        DestroyWindow();
        return false;
    }

    shuttingDown_ = false;
    rateStrip_.SetContextMenuOwner(GetSafeHwnd(), kRightClickSpeedTextMessage);
    rateStrip_.SetContextMenuEnabled(settings::IsRightClickSpeedTextEnabled());
    static_cast<void>(rateStrip_.Embed(
        core_.ShouldShowRateStrip(RateStrip::IsPrimaryBottomTaskbarAvailable())));
    if (ShouldRetryRateStrip())
    {
        ScheduleShellRecoveryRetry();
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
    CancelShellRecoveryRetry();
    rateStrip_.Shutdown();
    RemoveTrayIcon();

    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }
}



std::vector<winmon::NicSnapshot> TrayMessageWindow::ReadNicSnapshots(bool classifyForMenu) const
{
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || table == nullptr) return {};
    std::vector<winmon::NicSnapshot> snapshots;
    const auto classicConnectionIds = classifyForMenu ? ReadClassicConnectionIds() : ClassicConnectionIds{};
    snapshots.reserve(table->NumEntries);
    for (ULONG index = 0; index < table->NumEntries; ++index)
    {
        const auto& row = table->Table[index];
        winmon::NicSnapshot snapshot;
        snapshot.stableId = std::to_string(row.InterfaceLuid.Value);
        snapshot.friendlyName = row.Alias;
        snapshot.description = row.Description;
        snapshot.loopback = row.Type == IF_TYPE_SOFTWARE_LOOPBACK;
        snapshot.visibleInClassicConnections = !classifyForMenu || !classicConnectionIds.available || ContainsConnectionId(classicConnectionIds.values, row.InterfaceGuid);
        snapshot.up = row.OperStatus == IfOperStatusUp;
        snapshot.inOctets = row.InOctets;
        snapshot.outOctets = row.OutOctets;
        snapshots.push_back(std::move(snapshot));
    }
    FreeMibTable(table);
    return snapshots;
}

bool TrayMessageWindow::ShouldRetryRateStrip() const noexcept
{
    return rateStrip_.GetSafeHwnd() == nullptr &&
        core_.ShouldShowRateStrip(RateStrip::IsPrimaryBottomTaskbarAvailable());
}
void TrayMessageWindow::ScheduleShellRecoveryRetry() noexcept
{
    if (shuttingDown_ || shellRecoveryTimerActive_ || GetSafeHwnd() == nullptr)
    {
        return;
    }
    shellRecoveryTimerActive_ = SetTimer(kShellRecoveryTimer, kShellRecoveryCadenceMs, nullptr) != 0;
}

void TrayMessageWindow::CancelShellRecoveryRetry() noexcept
{
    if (!shellRecoveryTimerActive_)
    {
        return;
    }
    KillTimer(kShellRecoveryTimer);
    shellRecoveryTimerActive_ = false;
}

void TrayMessageWindow::RecoverRateStrip()
{
    rateStrip_.Shutdown();
    if (!rateStrip_.Embed(core_.ShouldShowRateStrip(RateStrip::IsPrimaryBottomTaskbarAvailable())))
    {
        if (ShouldRetryRateStrip()) ScheduleShellRecoveryRetry();
        return;
    }
    CancelShellRecoveryRetry();
}

void TrayMessageWindow::AttemptShellRecovery()
{
    if (shuttingDown_)
    {
        return;
    }

    if (!trayIconAdded_ && !AddTrayIcon())
    {
        ScheduleShellRecoveryRetry();
        return;
    }

    // Embed() revalidates the parent taskbar, so an Explorer-created stale
    // child is discarded before a new child is created.
    if (!rateStrip_.Embed(core_.ShouldShowRateStrip(RateStrip::IsPrimaryBottomTaskbarAvailable())))
    {
        if (ShouldRetryRateStrip()) ScheduleShellRecoveryRetry();
        return;
    }
    CancelShellRecoveryRetry();
}

LRESULT TrayMessageWindow::OnTaskbarCreated(WPARAM, LPARAM)
{
    if (shuttingDown_)
    {
        return 0;
    }

    // The shell has discarded the old notification-area state. Delete first
    // so a delayed shell callback cannot leave two icons after NIM_ADD.
    RemoveTrayIcon();
    static_cast<void>(AddTrayIcon());
    RecoverRateStrip();
    if (!trayIconAdded_ || ShouldRetryRateStrip())
    {
        ScheduleShellRecoveryRetry();
    }
    return 0;
}

LRESULT TrayMessageWindow::OnDpiChanged(WPARAM, LPARAM)
{
    RecoverRateStrip();
    return 0;
}

void TrayMessageWindow::OnDisplayChange(UINT, int, int)
{
    RecoverRateStrip();
}

void TrayMessageWindow::OnSettingChange(UINT, LPCTSTR)
{
    UpdateTrayIconTheme();
    RecoverRateStrip();
}

LRESULT TrayMessageWindow::OnThemeChanged()
{
    UpdateTrayIconTheme();
    RecoverRateStrip();
    return 0;
}

void TrayMessageWindow::SampleRates()
{
    snapshots_ = ReadNicSnapshots();
    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const double seconds = std::chrono::duration<double>(now).count();
    const auto display = core_.Sample(snapshots_, seconds);
    rateStrip_.SetRates(display.uploadText, display.downloadText);
}

void TrayMessageWindow::OnTimer(UINT_PTR timerId)
{
    if (timerId == kRateSampleTimer)
    {
        SampleRates();
        if (rateStrip_.GetSafeHwnd() != nullptr)
        {
            static_cast<void>(rateStrip_.Refresh());
        }
        if ((!trayIconAdded_ || ShouldRetryRateStrip()) && !shellRecoveryTimerActive_)
        {
            AttemptShellRecovery();
        }
    }
    else if (timerId == kShellRecoveryTimer)
    {
        shellRecoveryTimerActive_ = false;
        KillTimer(kShellRecoveryTimer);
        AttemptShellRecovery();
        if (!trayIconAdded_ || ShouldRetryRateStrip())
        {
            ScheduleShellRecoveryRetry();
        }
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

bool TrayMessageWindow::AddTrayIcon()
{
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

UINT TrayMessageWindow::ShowOperatorMenu()
{
    // TrackPopupMenu pumps messages, so a queued open request would otherwise
    // nest a second menu and rebuild menuNicIds_ under the outer one.
    if (menuOpen_) return 0;
    CPoint cursorPosition;
    if (!GetCursorPos(&cursorPosition)) return 0;
    const MenuOpenScope menuScope{menuOpen_};
    snapshots_ = ReadNicSnapshots(true);
    autostartWasEnabled_ = autostart::IsEnabled();
    const auto model = core_.BuildOperatorMenu(
        snapshots_, {autostartWasEnabled_, rateStrip_.IsContextMenuEnabled()});
    CMenu menu;
    CMenu nicMenu;
    if (!menu.CreatePopupMenu() || !nicMenu.CreatePopupMenu()) return 0;
    menuNicIds_.clear();
    UINT nextCommand = ID_OPERATOR_NETWORK_BASE;
    constexpr UINT allCommand = ID_OPERATOR_ALL_NETWORKS;
    bool nicSubmenuAttached = false;
    for (const auto& item : model)
    {
        switch (item.kind)
        {
        case winmon::OperatorMenuItemKind::All:
            if (!nicMenu.AppendMenu(MF_STRING, allCommand, item.label.c_str())) return 0;
            if (item.checked) nicMenu.CheckMenuRadioItem(allCommand, allCommand, allCommand, MF_BYCOMMAND);
            break;
        case winmon::OperatorMenuItemKind::Nic:
            if (!nicMenu.AppendMenu(MF_STRING, nextCommand, item.label.c_str())) return 0;
            menuNicIds_.push_back(item.stableId);
            if (item.checked) nicMenu.CheckMenuRadioItem(allCommand, nextCommand, nextCommand, MF_BYCOMMAND);
            ++nextCommand;
            break;
        case winmon::OperatorMenuItemKind::Separator:
            if (!nicSubmenuAttached)
            {
                if (!menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(nicMenu.GetSafeHmenu()), L"Network to Monitor")) return 0;
                nicMenu.Detach();
                nicSubmenuAttached = true;
            }
            if (!menu.AppendMenu(MF_SEPARATOR)) return 0;
            break;
        case winmon::OperatorMenuItemKind::Autostart:
            if (!AppendToggle(menu, ID_OPERATOR_AUTOSTART, item)) return 0;
            break;
        case winmon::OperatorMenuItemKind::RightClickSpeedText:
            if (!AppendToggle(menu, ID_OPERATOR_SPEED_TEXT_MENU, item)) return 0;
            break;
        case winmon::OperatorMenuItemKind::Exit:
            if (!menu.AppendMenu(MF_STRING, ID_OPERATOR_EXIT, item.label.c_str())) return 0;
            break;
        }
    }
    SetForegroundWindow();
    const UINT command = static_cast<UINT>(::TrackPopupMenu(menu.GetSafeHmenu(), TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY, cursorPosition.x, cursorPosition.y, 0, GetSafeHwnd(), nullptr));
    PostMessage(WM_NULL);
    return command;
}

void TrayMessageWindow::RequestExit()
{
    Shutdown();
    PostQuitMessage(0);
}
void TrayMessageWindow::HandleOperatorMenuCommand(UINT command)
{
    if (command == ID_OPERATOR_EXIT)
    {
        RequestExit();
    }
    else if (command == ID_OPERATOR_AUTOSTART)
    {
        if (autostartWasEnabled_) static_cast<void>(autostart::Disable());
        else static_cast<void>(autostart::Enable());
    }
    else if (command == ID_OPERATOR_SPEED_TEXT_MENU)
    {
        const bool enabled = !rateStrip_.IsContextMenuEnabled();
        rateStrip_.SetContextMenuEnabled(enabled);
        static_cast<void>(settings::SetRightClickSpeedTextEnabled(enabled));
    }
    else if (command == ID_OPERATOR_ALL_NETWORKS)
    {
        core_.SelectAll();
    }
    else if (command >= ID_OPERATOR_NETWORK_BASE)
    {
        const auto nicIndex = static_cast<std::size_t>(command - ID_OPERATOR_NETWORK_BASE);
        if (nicIndex < menuNicIds_.size()) core_.SelectNic(menuNicIds_[nicIndex]);
    }
}

LRESULT TrayMessageWindow::OnTrayNotification(WPARAM, LPARAM lParam)
{
    const UINT notification = LOWORD(lParam);
    if (notification != WM_RBUTTONUP && notification != WM_CONTEXTMENU) return 0;
    HandleOperatorMenuCommand(ShowOperatorMenu());
    return 0;
}

LRESULT TrayMessageWindow::OnRightClickSpeedText(WPARAM, LPARAM)
{
    if (shuttingDown_) return 0;
    HandleOperatorMenuCommand(ShowOperatorMenu());
    return 0;
}
