// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#include "app/TrayMessageWindow.h"
#include "platform/LaunchAtLogin.h"
#include "settings/OperatorSettings.h"

#include "res/resource.h"

#include <shellapi.h>
#include <vector>
#include <string>
#include <utility>
namespace
{
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayNotificationMessage = WM_APP + 1;
constexpr UINT kDisplayContextMenuMessage = WM_APP + 2;
constexpr UINT kFloatingRateDisplayMovedMessage = WM_APP + 3;
constexpr wchar_t kTrayTooltip[] = L"Win Mon";
constexpr UINT_PTR kRateSampleTimer = 1;
constexpr UINT_PTR kShellRecoveryTimer = 2;
constexpr UINT kShellRecoveryCadenceMs = 2000;
const UINT kTaskbarCreatedMessage = RegisterWindowMessageW(L"TaskbarCreated");
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
        // A failed menu build or an early return must release the core's menu state.
        if (!completed) core.CancelOperatorMenu();
    }
    OperatorMenuScope(const OperatorMenuScope&) = delete;
    OperatorMenuScope& operator=(const OperatorMenuScope&) = delete;

    winmon::WinMonCore& core;
    bool completed = false;
};
}

BEGIN_MESSAGE_MAP(TrayMessageWindow, CWnd)
    ON_MESSAGE(kTrayNotificationMessage, &TrayMessageWindow::OnTrayNotification)
    ON_MESSAGE(kDisplayContextMenuMessage, &TrayMessageWindow::OnDisplayContextMenu)
    ON_MESSAGE(kFloatingRateDisplayMovedMessage, &TrayMessageWindow::OnFloatingRateDisplayMoved)
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
    const auto startup = settings::LoadStartupToggles();
    taskbarTextVisible_ = startup.taskbarTextVisible;
    trayIconVisible_ = startup.trayIconVisible;
    floatingDisplayEnabled_ = startup.floatingDisplayEnabled;
    rateStrip_.SetContextMenuOwner(GetSafeHwnd(), kDisplayContextMenuMessage);
    rateStrip_.SetContextMenuEnabled(startup.taskbarContextMenuEnabled);
    static_cast<void>(rateStrip_.SetTaskbarDisplayOnLeft(startup.taskbarDisplayOnLeft));
    static_cast<void>(floatingRateDisplay_.SetTextVisible(startup.floatingTextVisible));
    static_cast<void>(floatingRateDisplay_.SetChartVisible(startup.floatingChartsVisible));
    static_cast<void>(floatingRateDisplay_.SetOpacity(startup.floatingDisplayOpacity));
    floatingRateDisplay_.SetContextMenuOwner(GetSafeHwnd(), kDisplayContextMenuMessage);
    floatingRateDisplay_.SetVisibilityChangeHandler([this](bool text, bool visible) {
        HandleOperatorMenuAction({text
            ? (visible ? winmon::OperatorActionKind::ShowFloatingText : winmon::OperatorActionKind::HideFloatingText)
            : (visible ? winmon::OperatorActionKind::ShowFloatingCharts : winmon::OperatorActionKind::HideFloatingCharts)});
        return (text ? floatingRateDisplay_.IsTextVisible() : floatingRateDisplay_.IsChartVisible()) == visible;
    });
    floatingRateDisplay_.SetPositionChangedOwner(GetSafeHwnd(), kFloatingRateDisplayMovedMessage);
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
    if (floatingDisplayEnabled_)
    {
        POINT position{};
        if (floatingRateDisplay_.ShowFloating(
                position,
                settings::GetFloatingDisplayPosition(position)))
        {
            SaveFloatingDisplayPosition();
        }
    }
    MaintainMenuAccess();
    return true;
}

void TrayMessageWindow::Shutdown() noexcept
{
    shuttingDown_ = true;
    KillTimer(kRateSampleTimer);
    floatingRateDisplay_.Shutdown();
    shellLifecycle_.Shutdown();

    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }
}

LRESULT TrayMessageWindow::OnTaskbarCreated(WPARAM, LPARAM)
{
    shellLifecycle_.OnShellCreated();
    MaintainMenuAccess();
    return 0;
}

LRESULT TrayMessageWindow::OnDpiChanged(WPARAM, LPARAM)
{
    shellLifecycle_.OnEnvironmentChanged();
    MaintainMenuAccess();
    return 0;
}

void TrayMessageWindow::OnDisplayChange(UINT, int, int)
{
    shellLifecycle_.OnEnvironmentChanged();
    MaintainMenuAccess();
}

void TrayMessageWindow::OnSettingChange(UINT, LPCTSTR section)
{
    UpdateTrayIconTheme();
    if (section && wcscmp(section, L"ImmersiveColorSet") == 0)
        shellLifecycle_.OnRateSample();
    else
        shellLifecycle_.OnEnvironmentChanged();
    MaintainMenuAccess();
}

LRESULT TrayMessageWindow::OnThemeChanged()
{
    UpdateTrayIconTheme();
    shellLifecycle_.OnRateSample();
    MaintainMenuAccess();
    return 0;
}

void TrayMessageWindow::SampleRates()
{
    core_.ObserveNetwork(networkReader_.Read());
    const auto display = core_.Sample();
    if (displayedNetworkGeneration_ != display.networkGeneration)
    {
        floatingRateDisplay_.ResetHistory();
        displayedNetworkGeneration_ = display.networkGeneration;
    }
    rateStrip_.SetRates(
        display.uploadText,
        display.downloadText);
    floatingRateDisplay_.SetRates(
        display.uploadText,
        display.downloadText,
        display.uploadBytesPerSecond,
        display.downloadBytesPerSecond);
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
    MaintainMenuAccess();
    CWnd::OnTimer(timerId);
}

HICON TrayMessageWindow::LoadTrayIcon() const noexcept
{
    const int resourceId = settings::IsLightTheme()
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
    if (!taskbarTextVisible_) return winmon::RateStripState::Hidden;
    if (!RateStrip::IsPrimaryBottomTaskbarAvailable())
    {
        return winmon::RateStripState::Hidden;
    }
    if (!rateStrip_.Embed(true)) return winmon::RateStripState::Retry;
    return rateStrip_.IsDisplayVisible() ? winmon::RateStripState::Visible : winmon::RateStripState::Hidden;
}

winmon::RateStripState TrayMessageWindow::RefreshRateStrip()
{
    if (!taskbarTextVisible_)
    {
        rateStrip_.Shutdown();
        return winmon::RateStripState::Hidden;
    }
    if (rateStrip_.GetSafeHwnd() != nullptr && rateStrip_.Refresh())
    {
        return rateStrip_.IsDisplayVisible() ? winmon::RateStripState::Visible : winmon::RateStripState::Hidden;
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

void TrayMessageWindow::SaveFloatingDisplayPosition()
{
    if (floatingRateDisplay_.GetSafeHwnd() != nullptr)
    {
        static_cast<void>(settings::SetFloatingDisplayPosition(
            floatingRateDisplay_.GetFloatingPosition()));
    }
}

bool TrayMessageWindow::EnsureTrayIcon()
{
    if (!winmon::ShouldShowTrayIcon(MenuToggles()))
    {
        RemoveTrayIcon();
        return !trayIconAdded_;
    }
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
    if (Shell_NotifyIconW(NIM_DELETE, &iconData)) trayIconAdded_ = false;
}

winmon::OperatorMenuToggles TrayMessageWindow::MenuToggles() const
{
    const HWND floating = floatingRateDisplay_.GetSafeHwnd();
    return {.startAtSignInEnabled = autostart::IsEnabled(),
        .taskbarContextMenuEnabled = rateStrip_.IsContextMenuEnabled(),
        .floatingDisplayEnabled = floatingDisplayEnabled_,
        .taskbarDisplayOnLeft = rateStrip_.IsTaskbarDisplayOnLeft(),
        .floatingTextVisible = floatingRateDisplay_.IsTextVisible(),
        .floatingChartsVisible = floatingRateDisplay_.IsChartVisible(),
        .floatingDisplayOpacity = floatingRateDisplay_.GetOpacity(),
        .taskbarTextVisible = taskbarTextVisible_,
        .trayIconVisible = trayIconVisible_,
        .taskbarDisplayAvailable = rateStrip_.IsDisplayVisible(),
        .floatingDisplayAvailable = floating != nullptr && ::IsWindowVisible(floating)};
}

void TrayMessageWindow::MaintainMenuAccess()
{
    if (!shuttingDown_ && !EnsureTrayIcon()) ScheduleRecovery();
}

winmon::OperatorAction TrayMessageWindow::ShowOperatorMenu()
{
    CPoint cursorPosition;
    if (!GetCursorPos(&cursorPosition)) return {};
    core_.ObserveNetwork(networkReader_.Read());
    MaintainMenuAccess();
    const auto model = core_.BeginOperatorMenu(MenuToggles());
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

void TrayMessageWindow::HandleOperatorMenuAction(winmon::OperatorAction action)
{
    if (!winmon::CanApplyOperatorAction(action.kind, MenuToggles())) return;
    switch (action.kind)
    {
    case winmon::OperatorActionKind::ShowTaskbarText:
    case winmon::OperatorActionKind::HideTaskbarText:
    {
        const bool previous = taskbarTextVisible_;
        const bool visible = action.kind == winmon::OperatorActionKind::ShowTaskbarText;
        static_cast<void>(winmon::OperatorSettingTransaction::Commit(
            [&] { taskbarTextVisible_ = visible; shellLifecycle_.OnEnvironmentChanged(); return true; },
            [&] { return settings::SetTaskbarTextVisible(visible); },
            [&] { taskbarTextVisible_ = previous; shellLifecycle_.OnEnvironmentChanged(); return true; }));
        break;
    }
    case winmon::OperatorActionKind::ShowTrayIcon:
    case winmon::OperatorActionKind::HideTrayIcon:
    {
        const bool previous = trayIconVisible_;
        const bool visible = action.kind == winmon::OperatorActionKind::ShowTrayIcon;
        static_cast<void>(winmon::OperatorSettingTransaction::Commit(
            [&] { trayIconVisible_ = visible; return EnsureTrayIcon(); },
            [&] { return settings::SetTrayIconVisible(visible); },
            [&] { trayIconVisible_ = previous; return EnsureTrayIcon(); }));
        break;
    }
    case winmon::OperatorActionKind::EnableStartAtSignIn:
        static_cast<void>(autostart::Enable());
        break;
    case winmon::OperatorActionKind::DisableStartAtSignIn:
        static_cast<void>(autostart::Disable());
        break;
    case winmon::OperatorActionKind::EnableTaskbarContextMenu:
    case winmon::OperatorActionKind::DisableTaskbarContextMenu:
    {
        const bool previous = rateStrip_.IsContextMenuEnabled();
        const bool enabled = action.kind == winmon::OperatorActionKind::EnableTaskbarContextMenu;
        static_cast<void>(winmon::OperatorSettingTransaction::Commit(
            [&] { rateStrip_.SetContextMenuEnabled(enabled); return true; },
            [&] { return settings::SetTaskbarContextMenuEnabled(enabled); },
            [&] { rateStrip_.SetContextMenuEnabled(previous); return true; }));
        break;
    }
    case winmon::OperatorActionKind::ShowFloatingDisplay:
    case winmon::OperatorActionKind::HideFloatingDisplay:
    {
        const bool previous = floatingDisplayEnabled_;
        const bool enabled = action.kind == winmon::OperatorActionKind::ShowFloatingDisplay;
        const auto applyFloating = [&](bool show) {
            floatingDisplayEnabled_ = show;
            if (!show) { floatingRateDisplay_.Shutdown(); return true; }
            POINT position{};
            return floatingRateDisplay_.ShowFloating(position, settings::GetFloatingDisplayPosition(position));
        };
        if (winmon::OperatorSettingTransaction::Commit(
                [&] { return applyFloating(enabled); },
                [&] { return settings::SetFloatingDisplayEnabled(enabled); },
                [&] { return applyFloating(previous); }) == winmon::OperatorSettingOutcome::Applied)
        {
            SaveFloatingDisplayPosition();
        }
        break;
    }
    case winmon::OperatorActionKind::ShowFloatingText:
    case winmon::OperatorActionKind::HideFloatingText:
    {
        const bool previous = floatingRateDisplay_.IsTextVisible();
        const bool visible = action.kind == winmon::OperatorActionKind::ShowFloatingText;
        if (winmon::OperatorSettingTransaction::Commit(
                [&] { return floatingRateDisplay_.SetTextVisible(visible); },
                [&] { return settings::SetFloatingTextVisible(visible); },
                [&] { return floatingRateDisplay_.SetTextVisible(previous); }) == winmon::OperatorSettingOutcome::RecoveryRequired)
        {
            static_cast<void>(floatingRateDisplay_.SetTextVisible(settings::IsFloatingTextVisible()));
        }
        break;
    }
    case winmon::OperatorActionKind::ShowFloatingCharts:
    case winmon::OperatorActionKind::HideFloatingCharts:
    {
        const bool previous = floatingRateDisplay_.IsChartVisible();
        const bool visible = action.kind == winmon::OperatorActionKind::ShowFloatingCharts;
        if (winmon::OperatorSettingTransaction::Commit(
                [&] { return floatingRateDisplay_.SetChartVisible(visible); },
                [&] { return settings::SetFloatingChartsVisible(visible); },
                [&] { return floatingRateDisplay_.SetChartVisible(previous); }) == winmon::OperatorSettingOutcome::RecoveryRequired)
        {
            static_cast<void>(floatingRateDisplay_.SetChartVisible(settings::AreFloatingChartsVisible()));
        }
        break;
    }
    case winmon::OperatorActionKind::SetFloatingDisplayOpacity:
    {
        const int previous = floatingRateDisplay_.GetOpacity();
        static_cast<void>(winmon::OperatorSettingTransaction::Commit(
            [&] { return floatingRateDisplay_.SetOpacity(action.opacity); },
            [&] { return settings::SetFloatingDisplayOpacity(action.opacity); },
            [&] { return floatingRateDisplay_.SetOpacity(previous); }));
        break;
    }
    case winmon::OperatorActionKind::SetTaskbarDisplayLeft:
    case winmon::OperatorActionKind::SetTaskbarDisplayRight:
    {
        const bool previous = rateStrip_.IsTaskbarDisplayOnLeft();
        const bool left = action.kind == winmon::OperatorActionKind::SetTaskbarDisplayLeft;
        if (previous == left) break;
        const auto outcome = winmon::OperatorSettingTransaction::Commit(
            [&] { return rateStrip_.SetTaskbarDisplayOnLeft(left); },
            [&] { return settings::SetTaskbarDisplayOnLeft(left); },
            [&] { return rateStrip_.SetTaskbarDisplayOnLeft(previous); });
        if (outcome == winmon::OperatorSettingOutcome::RecoveryRequired)
        {
            rateStrip_.Shutdown();
            static_cast<void>(rateStrip_.SetTaskbarDisplayOnLeft(settings::IsTaskbarDisplayOnLeft()));
            shellLifecycle_.OnEnvironmentChanged();
        }
        if (outcome != winmon::OperatorSettingOutcome::Applied)
        {
            MessageBoxW(L"Win Mon could not change the speed text position.", L"Win Mon", MB_OK | MB_ICONERROR);
        }
        break;
    }
    case winmon::OperatorActionKind::Exit:
        RequestExit();
        break;
    case winmon::OperatorActionKind::None:
        break;
    }
    MaintainMenuAccess();
}

LRESULT TrayMessageWindow::OnTrayNotification(WPARAM, LPARAM lParam)
{
    const UINT notification = LOWORD(lParam);
    if (notification != WM_RBUTTONUP && notification != WM_CONTEXTMENU) return 0;
    HandleOperatorMenuAction(ShowOperatorMenu());
    return 0;
}

LRESULT TrayMessageWindow::OnDisplayContextMenu(WPARAM, LPARAM)
{
    if (shuttingDown_) return 0;
    HandleOperatorMenuAction(ShowOperatorMenu());
    return 0;
}

LRESULT TrayMessageWindow::OnFloatingRateDisplayMoved(WPARAM, LPARAM)
{
    SaveFloatingDisplayPosition();
    return 0;
}
