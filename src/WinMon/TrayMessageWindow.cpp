#include "TrayMessageWindow.h"

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
}

BEGIN_MESSAGE_MAP(TrayMessageWindow, CWnd)
    ON_MESSAGE(kTrayNotificationMessage, &TrayMessageWindow::OnTrayNotification)
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
    CPoint cursorPosition;
    if (!GetCursorPos(&cursorPosition)) return 0;
    snapshots_ = ReadNicSnapshots(true);
    const auto model = core_.BuildOperatorMenu(snapshots_);
    CMenu menu;
    CMenu nicMenu;
    if (!menu.CreatePopupMenu() || !nicMenu.CreatePopupMenu()) return 0;
    menuNicIds_.clear();
    UINT nextCommand = ID_OPERATOR_NIC_BASE;
    UINT allCommand = ID_OPERATOR_ALL;
    UINT exitCommand = ID_OPERATOR_EXIT;
    for (const auto& item : model)
    {
        if (item.kind == winmon::OperatorMenuItemKind::All)
        {
            if (!nicMenu.AppendMenu(MF_STRING, allCommand, item.label.c_str())) return 0;
            if (item.checked) nicMenu.CheckMenuRadioItem(allCommand, allCommand, allCommand, MF_BYCOMMAND);
        }
        else if (item.kind == winmon::OperatorMenuItemKind::Nic)
        {
            if (!nicMenu.AppendMenu(MF_STRING, nextCommand, item.label.c_str())) return 0;
            menuNicIds_.push_back(item.stableId);
            if (item.checked) nicMenu.CheckMenuRadioItem(allCommand, nextCommand, nextCommand, MF_BYCOMMAND);
            ++nextCommand;
        }
        else if (item.kind == winmon::OperatorMenuItemKind::Separator)
        {
            if (!menu.AppendMenu(MF_POPUP, reinterpret_cast<UINT_PTR>(nicMenu.GetSafeHmenu()), L"NIC Selection")) return 0;
            nicMenu.Detach();
            if (!menu.AppendMenu(MF_SEPARATOR)) return 0;
        }
        else if (item.kind == winmon::OperatorMenuItemKind::Exit && !menu.AppendMenu(MF_STRING, exitCommand, item.label.c_str()))
        {
            return 0;
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
LRESULT TrayMessageWindow::OnTrayNotification(WPARAM, LPARAM lParam)
{
    const UINT notification = LOWORD(lParam);
    if (notification != WM_RBUTTONUP && notification != WM_CONTEXTMENU) return 0;
    const UINT command = ShowOperatorMenu();
    if (command == ID_OPERATOR_EXIT) RequestExit();
    else if (command == ID_OPERATOR_ALL) core_.SelectAll();
    else if (command >= ID_OPERATOR_NIC_BASE)
    {
        const auto nicIndex = static_cast<std::size_t>(command - ID_OPERATOR_NIC_BASE);
        if (nicIndex < menuNicIds_.size()) core_.SelectNic(menuNicIds_[nicIndex]);
    }
    return 0;
}
