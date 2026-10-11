// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include "taskbar/RateFont.h"
#include "WinMonCore.h"

template<class Strip>
class RateFontSettingChange final : public winmon::OperatorSettingChange
{
public:
    RateFontSettingChange(
        Strip& rateStrip,
        const RateFontSelection& previous,
        const RateFontSelection& selected, bool (*persist)(const RateFontSelection&)) noexcept
        : rateStrip_(rateStrip), previous_(previous), selected_(selected), persist_(persist) {}

    bool ApplyLive() override
    {
        return rateStrip_.SetRateFont(selected_);
    }
    bool Persist() override { return persist_(selected_); }
    bool RollbackLive() override
    {
        const bool stripRestored = rateStrip_.SetRateFont(previous_);
        return stripRestored;
    }

private:
    Strip& rateStrip_;
    RateFontSelection previous_;
    RateFontSelection selected_;
    bool (*persist_)(const RateFontSelection&);
};
