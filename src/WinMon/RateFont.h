#pragma once
#include <windows.h>
#include <optional>

struct RateFontSelection final
{
    LOGFONTW logFont{};
    int pointSizeTenths = 90;
};

// Shared selection and rollback rules; each display supplies its own redraw.
class RateFont final
{
public:
    bool Get(HWND window, RateFontSelection& selection) const noexcept
    {
        if (selected_)
        {
            selection = *selected_;
            return true;
        }
        const UINT dpi = window == nullptr ? GetDpiForSystem() : GetDpiForWindow(window);
        NONCLIENTMETRICSW metrics{};
        metrics.cbSize = sizeof(metrics);
        if (!SystemParametersInfoForDpi(SPI_GETNONCLIENTMETRICS, sizeof(metrics), &metrics, 0, dpi))
            return false;
        selection.logFont = metrics.lfMessageFont;
        const int height = selection.logFont.lfHeight;
        selection.pointSizeTenths = MulDiv(height < 0 ? -height : height, 720, static_cast<int>(dpi));
        return selection.logFont.lfFaceName[0] != L'\0' && selection.pointSizeTenths > 0;
    }

    template<class Redraw>
    bool Apply(std::optional<RateFontSelection> selection, Redraw redraw) noexcept
    {
        if (selection && (selection->logFont.lfFaceName[0] == L'\0' || selection->pointSizeTenths <= 0))
            return false;
        const auto previous = selected_;
        selected_ = selection;
        // Even an unchanged selection must retry a previously failed redraw.
        if (redraw()) return true;
        selected_ = previous;
        static_cast<void>(redraw());
        return false;
    }

private:
    std::optional<RateFontSelection> selected_;
};
