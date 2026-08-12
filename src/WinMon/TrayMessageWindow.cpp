#include "TrayMessageWindow.h"

#include "resource.h"

#include <shellapi.h>
#include <iphlpapi.h>
#include <netioapi.h>
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
}

BEGIN_MESSAGE_MAP(TrayMessageWindow, CWnd)
    ON_MESSAGE(kTrayNotificationMessage, &TrayMessageWindow::OnTrayNotification)
    ON_WM_TIMER()
END_MESSAGE_MAP()

bool TrayMessageWindow::Initialize()
{
    const CString windowClass = AfxRegisterWndClass(0);
    if (!CreateEx(
            0,
            windowClass,
            L"Win Mon Message Sink",
            0,
            0,
            0,
            0,
            0,
            HWND_MESSAGE,
            nullptr))
    {
        return false;
    }

    if (!AddTrayIcon())
    {
        DestroyWindow();
        return false;
    }

    static_cast<void>(rateStrip_.Embed());
    SetTimer(kRateSampleTimer, 1000, nullptr);
    return true;
}

void TrayMessageWindow::Shutdown() noexcept
{
    KillTimer(kRateSampleTimer);
    rateStrip_.Shutdown();
    RemoveTrayIcon();

    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }
}

void TrayMessageWindow::SampleRates()
{
    MIB_IF_TABLE2* table = nullptr;
    if (GetIfTable2(&table) != NO_ERROR || table == nullptr)
    {
        return;
    }

    std::vector<winmon::NicSnapshot> snapshots;
    snapshots.reserve(table->NumEntries);
    for (ULONG index = 0; index < table->NumEntries; ++index)
    {
        const auto& row = table->Table[index];
        winmon::NicSnapshot snapshot;
        snapshot.stableId = std::to_string(row.InterfaceLuid.Value);
        snapshot.name = row.Alias;
        snapshot.loopback = row.Type == IF_TYPE_SOFTWARE_LOOPBACK;
        snapshot.up = row.OperStatus == IfOperStatusUp;
        snapshot.inOctets = row.InOctets;
        snapshot.outOctets = row.OutOctets;
        snapshots.push_back(std::move(snapshot));
    }
    FreeMibTable(table);

    const auto now = std::chrono::steady_clock::now().time_since_epoch();
    const double seconds = std::chrono::duration<double>(now).count();
    const auto display = core_.Sample(snapshots, seconds);
    rateStrip_.SetRates(display.uploadText, display.downloadText);
}

void TrayMessageWindow::OnTimer(UINT_PTR timerId)
{
    if (timerId == kRateSampleTimer)
    {
        SampleRates();
    }
    CWnd::OnTimer(timerId);
}

bool TrayMessageWindow::AddTrayIcon()
{
    NOTIFYICONDATAW iconData{};
    iconData.cbSize = sizeof(iconData);
    iconData.hWnd = GetSafeHwnd();
    iconData.uID = kTrayIconId;
    iconData.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP | NIF_SHOWTIP;
    iconData.uCallbackMessage = kTrayNotificationMessage;
    iconData.hIcon = static_cast<HICON>(::LoadImageW(
        AfxGetResourceHandle(),
        MAKEINTRESOURCEW(IDI_WINMON),
        IMAGE_ICON,
        0,
        0,
        LR_DEFAULTSIZE | LR_SHARED));

    if (iconData.hIcon == nullptr)
    {
        return false;
    }

    wcscpy_s(iconData.szTip, kTrayTooltip);

    if (!Shell_NotifyIconW(NIM_ADD, &iconData))
    {
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
    if (!GetCursorPos(&cursorPosition))
    {
        return 0;
    }

    CMenu menu;
    if (!menu.CreatePopupMenu())
    {
        return 0;
    }

    menu.AppendMenu(MF_STRING, ID_OPERATOR_ALL, L"All");
    menu.CheckMenuRadioItem(
        ID_OPERATOR_ALL,
        ID_OPERATOR_ALL,
        ID_OPERATOR_ALL,
        MF_BYCOMMAND);
    menu.AppendMenu(MF_SEPARATOR);
    menu.AppendMenu(MF_STRING, ID_OPERATOR_EXIT, L"Exit");

    SetForegroundWindow();
    const UINT command = static_cast<UINT>(::TrackPopupMenu(
        menu.GetSafeHmenu(),
        TPM_RIGHTBUTTON | TPM_RETURNCMD | TPM_NONOTIFY,
        cursorPosition.x,
        cursorPosition.y,
        0,
        GetSafeHwnd(),
        nullptr));
    PostMessage(WM_NULL);
    return command;
}

void TrayMessageWindow::RequestExit()
{
    rateStrip_.Shutdown();
    RemoveTrayIcon();

    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
    }

    PostQuitMessage(0);
}

LRESULT TrayMessageWindow::OnTrayNotification(WPARAM, LPARAM lParam)
{
    const UINT notification = LOWORD(lParam);
    if (notification != WM_RBUTTONUP && notification != WM_CONTEXTMENU)
    {
        return 0;
    }

    if (ShowOperatorMenu() == ID_OPERATOR_EXIT)
    {
        RequestExit();
    }

    return 0;
}
