#include "RateFontSettingChange.h"
#include <iostream>
#include <stdexcept>

void Require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int savedSize = 90;
bool canSave = true;
bool Persist(const RateFontSelection& font)
{
    if (canSave) savedSize = font.pointSizeTenths;
    return canSave;
}

struct Display
{
    RateFont font;
    int rejectedSize = 0;
    int renderedSize = 0;
    bool SetRateFont(const RateFontSelection& selection)
    {
        return font.Apply(selection, [this] {
            RateFontSelection current;
            if (!font.Get(nullptr, current) || current.pointSizeTenths == rejectedSize) return false;
            renderedSize = current.pointSizeTenths;
            return true;
        });
    }
};

int main()
{
    try
    {
        RateFontSelection oldFont;
        wcscpy_s(oldFont.logFont.lfFaceName, L"Segoe UI");
        auto newFont = oldFont;
        newFont.pointSizeTenths = 100;
        Display strip, floating;
        Require(strip.SetRateFont(oldFont) && floating.SetRateFont(oldFont), "initial fonts");
        floating.rejectedSize = 100;
        RateFontSettingChange change{strip, floating, oldFont, newFont, Persist};
        using winmon::OperatorSettingOutcome;
        using winmon::OperatorSettingTransaction;
        Require(OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::LiveApplicationFailed,
            "second display failure rejects font change");
        Require(strip.renderedSize == 90 && floating.renderedSize == 90 && savedSize == 90,
            "partial application restores both rendered fonts without saving");
        floating.rejectedSize = 0;
        canSave = false;
        Require(OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::RolledBack,
            "save failure rolls back fonts");
        Require(strip.renderedSize == 90 && floating.renderedSize == 90 && savedSize == 90,
            "save failure leaves both displays and storage unchanged");
        strip.rejectedSize = 90;
        Require(OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::RecoveryRequired,
            "failed first restoration requests recovery");
        Require(strip.renderedSize == 100 && floating.renderedSize == 90 && savedSize == 90,
            "second restoration still runs when first restoration fails");
        strip.rejectedSize = 0;
        Require(change.RollbackLive(), "later retry restores both displays");
        Require(strip.renderedSize == 90 && floating.renderedSize == 90, "retry converges to old font");
        canSave = true;
        Require(OperatorSettingTransaction::Commit(change) == OperatorSettingOutcome::Applied,
            "successful font change");
        Require(strip.renderedSize == 100 && floating.renderedSize == 100 && savedSize == 100,
            "successful change updates displays and storage");
        auto invalid = newFont;
        invalid.logFont.lfFaceName[0] = L'\0';
        Require(!strip.SetRateFont(invalid) && strip.renderedSize == 100, "invalid font preserves rendered font");
        int redraws = 0;
        Require(!strip.font.Apply(newFont, [&] { ++redraws; return false; }), "unchanged font still reports redraw failure");
        Require(redraws == 2, "unchanged font attempts apply and restoration");
        RateFontSelection systemFont, resetFont;
        RateFont defaults;
        Require(defaults.Get(nullptr, systemFont), "system default font");
        Require(strip.font.Apply(std::nullopt, [] { return true; }) && strip.font.Get(nullptr, resetFont), "reset to default font");
        Require(wcscmp(systemFont.logFont.lfFaceName, resetFont.logFont.lfFaceName) == 0 &&
            systemFont.pointSizeTenths == resetFont.pointSizeTenths, "reset follows system default");
        std::cout << "Rate font checks passed\n";
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
