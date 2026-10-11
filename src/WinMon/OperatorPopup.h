#pragma once
#include <afxwin.h>
#include <afxcmn.h>
#include "../WinMonCore/WinMonCore.h"
#include <functional>
#include <memory>

// The main menu hosts a real trackbar; nested choice lists remain native menus.
class OperatorPopup final : public CWnd
{
public:
    std::uint32_t Track(CWnd& owner, POINT position,
        const std::vector<winmon::OperatorMenuItem>& items, int opacity,
        std::function<bool(int)> preview);
private:
    LRESULT WindowProc(UINT message, WPARAM wParam, LPARAM lParam) override;
    void Choose(size_t index);
    std::vector<winmon::OperatorMenuItem> items_;
    std::vector<std::unique_ptr<CButton>> buttons_;
    CSliderCtrl slider_;
    CStatic opacityLabel_;
    CFont font_;
    CBrush background_{RGB(250,250,250)};
    std::function<bool(int)> preview_;
    std::uint32_t choice_ = 0;
    int opacity_ = 100;
    int sliderTop_ = 0;
    UINT dpi_ = 96;
    bool submenu_ = false;
};
