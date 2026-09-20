#pragma once
#include "RateFont.h"
#include "../WinMonCore/WinMonCore.h"

template<class Strip, class Floating>
class RateFontSettingChange final : public winmon::OperatorSettingChange
{
public:
    RateFontSettingChange(
        Strip& rateStrip,
        Floating& floatingRateDisplay,
        const RateFontSelection& previous,
        const RateFontSelection& selected, bool (*persist)(const RateFontSelection&)) noexcept
        : rateStrip_(rateStrip), floatingRateDisplay_(floatingRateDisplay), previous_(previous), selected_(selected), persist_(persist) {}

    bool ApplyLive() override
    {
        return rateStrip_.SetRateFont(selected_) && floatingRateDisplay_.SetRateFont(selected_);
    }
    bool Persist() override { return persist_(selected_); }
    bool RollbackLive() override
    {
        const bool stripRestored = rateStrip_.SetRateFont(previous_);
        const bool floatingRestored = floatingRateDisplay_.SetRateFont(previous_);
        return stripRestored && floatingRestored;
    }

private:
    Strip& rateStrip_;
    Floating& floatingRateDisplay_;
    RateFontSelection previous_;
    RateFontSelection selected_;
    bool (*persist_)(const RateFontSelection&);
};
