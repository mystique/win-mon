#include "TrayMessageWindow.h"

#include "resource.h"

#include <shellapi.h>

namespace
{
constexpr UINT kTrayIconId = 1;
constexpr UINT kTrayNotificationMessage = WM_APP + 1;
constexpr wchar_t kTrayTooltip[] = L"Win Mon";
}

BEGIN_MESSAGE_MAP(TrayMessageWindow, CWnd)
    ON_MESSAGE(kTrayNotificationMessage, &TrayMessageWindow::OnTrayNotification)
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
    return true;
}

void TrayMessageWindow::Shutdown() noexcept
{
    rateStrip_.Shutdown();
    RemoveTrayIcon();

    if (GetSafeHwnd() != nullptr)
    {
        DestroyWindow();
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
