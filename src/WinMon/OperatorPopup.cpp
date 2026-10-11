#include "OperatorPopup.h"
#include <algorithm>
#pragma comment(linker,"/manifestdependency:\"type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='amd64' publicKeyToken='6595b64144ccf1df' language='*'\"")

namespace
{
constexpr UINT kFirstButton = 100;
constexpr UINT kSlider = 5000;
bool AppendItems(CMenu& menu, const std::vector<winmon::OperatorMenuItem>& items)
{
    for (const auto& item : items)
    {
        if (item.separator) { if (!menu.AppendMenu(MF_SEPARATOR)) return false; continue; }
        if (!item.children.empty())
        {
            CMenu child;
            if (!child.CreatePopupMenu() || !AppendItems(child,item.children) ||
                !menu.AppendMenu(MF_POPUP,reinterpret_cast<UINT_PTR>(child.GetSafeHmenu()),item.label.c_str())) return false;
            child.Detach();
        }
        else if (!menu.AppendMenu(MF_STRING | (item.checked ? MF_CHECKED : 0) |
                (item.radio ? MFT_RADIOCHECK : 0) | (item.enabled ? 0 : MF_GRAYED),
                item.choiceToken,item.label.c_str())) return false;
    }
    return true;
}
}

std::uint32_t OperatorPopup::Track(CWnd& owner, POINT position,
    const std::vector<winmon::OperatorMenuItem>& items, int opacity, std::function<bool(int)> preview)
{
    items_ = items;
    opacity_ = opacity;
    preview_ = std::move(preview);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};
    if (!InitCommonControlsEx(&controls)) return 0;
    const CString cls = AfxRegisterWndClass(CS_DROPSHADOW,LoadCursorW(nullptr,IDC_ARROW));
    if (!CreateEx(WS_EX_TOOLWINDOW | WS_EX_CONTROLPARENT,cls,L"Win Mon Menu",WS_POPUP | WS_BORDER | WS_CLIPCHILDREN,
            CRect(position.x,position.y,position.x+340,position.y+500),&owner,0)) return 0;
    dpi_ = GetDpiForWindow(GetSafeHwnd());
    const auto dip = [&](int value) { return MulDiv(value,static_cast<int>(dpi_),96); };
    NONCLIENTMETRICSW metrics{sizeof(metrics)};
    if (SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS,sizeof(metrics),&metrics,0,dpi_))
        font_.CreateFontIndirect(&metrics.lfMenuFont);
    int width = dip(300);
    { CClientDC dc(this); auto* previous = font_.GetSafeHandle() ? dc.SelectObject(&font_) : nullptr;
      for (const auto& item : items_) width = std::max(width,static_cast<int>(dc.GetTextExtent(item.label.c_str()).cx)+dip(80));
      if (previous) dc.SelectObject(previous); }
    int top = dip(6);
    for (size_t i = 0; i < items_.size(); ++i)
    {
        const auto& item = items_[i];
        if (item.separator) { top += dip(9); continue; }
        if (item.label == L"Exit")
        {
            sliderTop_ = top;
            top += dip(76);
        }
        auto button = std::make_unique<CButton>();
        const std::wstring label = (item.checked ? L"\u2713 " : L"") + item.label;
        if (!button->Create(label.c_str(),WS_CHILD | WS_VISIBLE | WS_TABSTOP | BS_OWNERDRAW,
                CRect(dip(6),top,width-dip(6),top+dip(30)),this,kFirstButton+static_cast<UINT>(i)))
        { DestroyWindow(); return 0; }
        if (font_.GetSafeHandle()) button->SetFont(&font_);
        button->EnableWindow(item.enabled);
        buttons_.push_back(std::move(button));
        top += dip(30);
    }
    if (!slider_.Create(WS_CHILD | WS_VISIBLE | WS_TABSTOP | TBS_HORZ | TBS_NOTICKS,
            CRect(dip(16),sliderTop_+dip(29),width-dip(70),sliderTop_+dip(61)),this,kSlider))
    { DestroyWindow(); return 0; }
    slider_.SetRange(10,100);
    slider_.SetPos(opacity_);
    slider_.SetLineSize(1);
    slider_.SetPageSize(10);
    slider_.SetWindowTextW(L"Floating Opacity");
    const std::wstring value = std::to_wstring(opacity_)+L"%";
    if (!opacityLabel_.Create(value.c_str(),WS_CHILD | WS_VISIBLE | SS_CENTERIMAGE,
            CRect(width-dip(60),sliderTop_+dip(29),width-dip(12),sliderTop_+dip(61)),this))
    { DestroyWindow(); return 0; }
    if (font_.GetSafeHandle()) opacityLabel_.SetFont(&font_);
    MONITORINFO monitor{sizeof(monitor)};
    GetMonitorInfoW(MonitorFromPoint(position,MONITOR_DEFAULTTONEAREST),&monitor);
    const int height = top+dip(6);
    position.x = std::clamp(position.x,monitor.rcWork.left,std::max(monitor.rcWork.left,monitor.rcWork.right-width));
    position.y = std::clamp(position.y,monitor.rcWork.top,std::max(monitor.rcWork.top,monitor.rcWork.bottom-height));
    SetWindowPos(&wndTopMost,position.x,position.y,width,height,SWP_SHOWWINDOW);
    SetForegroundWindow();
    if (!buttons_.empty()) buttons_.front()->SetFocus();
    MSG message{};
    while (IsWindowVisible())
    {
        const BOOL result = GetMessageW(&message,nullptr,0,0);
        if (result <= 0) { if (result == 0) PostQuitMessage(static_cast<int>(message.wParam)); break; }
        if (message.message == WM_KEYDOWN && (message.hwnd == GetSafeHwnd() || ::IsChild(GetSafeHwnd(),message.hwnd)))
        {
            if (message.wParam == VK_ESCAPE) break;
            if (message.hwnd != slider_.GetSafeHwnd() && (message.wParam == VK_UP || message.wParam == VK_DOWN))
            {
                const int direction = message.wParam == VK_DOWN ? 1 : -1;
                int index = ::GetDlgCtrlID(message.hwnd)-static_cast<int>(kFirstButton);
                for (size_t step = 0; step < items_.size(); ++step)
                {
                    index = (index+direction+static_cast<int>(items_.size())) % static_cast<int>(items_.size());
                    if (items_[index].enabled && !items_[index].separator) { GetDlgItem(kFirstButton+index)->SetFocus(); break; }
                }
                continue;
            }
            if ((message.wParam == VK_RETURN || message.wParam == VK_RIGHT) && message.hwnd != slider_.GetSafeHwnd())
            {
                const int id = ::GetDlgCtrlID(message.hwnd);
                if (id >= static_cast<int>(kFirstButton) && id < static_cast<int>(kFirstButton+items_.size()) &&
                    (message.wParam == VK_RETURN || !items_[id-kFirstButton].children.empty())) Choose(id-kFirstButton);
                continue;
            }
        }
        if (!::IsDialogMessageW(GetSafeHwnd(),&message)) { TranslateMessage(&message); DispatchMessageW(&message); }
    }
    DestroyWindow();
    return choice_;
}

void OperatorPopup::Choose(size_t index)
{
    if (index >= items_.size() || !items_[index].enabled || items_[index].separator) return;
    const auto& item = items_[index];
    if (!item.children.empty())
    {
        CMenu child;
        if (!child.CreatePopupMenu() || !AppendItems(child,item.children)) return;
        CRect row;
        GetDlgItem(kFirstButton+static_cast<UINT>(index))->GetWindowRect(row);
        submenu_ = true;
        choice_ = static_cast<std::uint32_t>(::TrackPopupMenu(child.GetSafeHmenu(),TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON,
            row.right,row.top,0,GetSafeHwnd(),nullptr));
        submenu_ = false;
        if (!choice_)
        {
            if (::GetForegroundWindow() != GetSafeHwnd()) ShowWindow(SW_HIDE);
            return;
        }
    }
    else choice_ = item.choiceToken;
    ShowWindow(SW_HIDE);
}

LRESULT OperatorPopup::WindowProc(UINT message, WPARAM wParam, LPARAM lParam)
{
    if (message == WM_DRAWITEM)
    {
        const auto* draw = reinterpret_cast<const DRAWITEMSTRUCT*>(lParam);
        if (draw->CtlID < kFirstButton || draw->CtlID >= kFirstButton+items_.size()) return FALSE;
        const auto& item = items_[draw->CtlID-kFirstButton];
        CDC dc; dc.Attach(draw->hDC);
        CRect row(draw->rcItem);
        const bool highlighted = item.enabled && (draw->itemState & (ODS_SELECTED | ODS_FOCUS | ODS_HOTLIGHT));
        dc.FillSolidRect(row,highlighted ? RGB(225,237,250) : RGB(250,250,250));
        auto* previous = font_.GetSafeHandle() ? dc.SelectObject(&font_) : nullptr;
        dc.SetBkMode(TRANSPARENT); dc.SetTextColor(item.enabled ? RGB(25,25,25) : RGB(135,135,135));
        const int inset = MulDiv(28,static_cast<int>(dpi_),96);
        CRect mark = row; mark.right = mark.left+inset;
        if (item.checked) dc.DrawText(L"\u2713",mark,DT_CENTER | DT_VCENTER | DT_SINGLELINE);
        CRect label = row; label.left += inset; label.right -= inset;
        dc.DrawText(item.label.c_str(),label,DT_LEFT | DT_VCENTER | DT_SINGLELINE | DT_NOPREFIX);
        if (!item.children.empty()) { mark = row; mark.left = mark.right-inset; dc.DrawText(L"\u203a",mark,DT_CENTER | DT_VCENTER | DT_SINGLELINE); }
        if (previous) dc.SelectObject(previous);
        dc.Detach();
        return TRUE;
    }
    if (message == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) >= kFirstButton && LOWORD(wParam) < kFirstButton+items_.size())
    { Choose(LOWORD(wParam)-kFirstButton); return 0; }
    if (message == WM_HSCROLL && reinterpret_cast<HWND>(lParam) == slider_.GetSafeHwnd())
    {
        const int selected = slider_.GetPos();
        if (preview_(selected)) opacity_ = selected;
        else slider_.SetPos(opacity_);
        const std::wstring value = std::to_wstring(opacity_)+L"%";
        opacityLabel_.SetWindowTextW(value.c_str());
        Invalidate(FALSE);
        return 0;
    }
    if (message == WM_ACTIVATE && LOWORD(wParam) == WA_INACTIVE && !submenu_) ShowWindow(SW_HIDE);
    if (message == WM_CLOSE) { ShowWindow(SW_HIDE); return 0; }
    if (message == WM_ERASEBKGND) return 1;
    if (message == WM_CTLCOLORSTATIC || message == WM_CTLCOLORBTN)
    { SetBkColor(reinterpret_cast<HDC>(wParam),RGB(250,250,250)); return reinterpret_cast<LRESULT>(background_.GetSafeHandle()); }
    if (message == WM_PAINT)
    {
        CPaintDC dc(this); CRect rect; GetClientRect(rect); dc.FillSolidRect(rect,RGB(250,250,250));
        auto* previous = font_.GetSafeHandle() ? dc.SelectObject(&font_) : nullptr;
        dc.SetBkMode(TRANSPARENT); dc.SetTextColor(RGB(25,25,25));
        const auto dip = [&](int value) { return MulDiv(value,static_cast<int>(dpi_),96); };
        int top = dip(6);
        for (const auto& item : items_)
        {
            if (item.separator) { dc.FillSolidRect(dip(16),top+dip(4),rect.Width()-dip(32),1,RGB(220,220,220)); top += dip(9); }
            else { if (item.label == L"Exit") top += dip(76); top += dip(30); }
        }
        dc.TextOutW(dip(20),sliderTop_+dip(6),L"Floating Opacity");
        dc.FillSolidRect(dip(16),sliderTop_+dip(71),rect.Width()-dip(32),1,RGB(220,220,220));
        if (previous) dc.SelectObject(previous);
        return 0;
    }
    return CWnd::WindowProc(message,wParam,lParam);
}
