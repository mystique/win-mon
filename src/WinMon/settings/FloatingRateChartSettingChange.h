// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include "WinMonCore.h"
template<class Display>
class FloatingRateChartSettingChange final : public winmon::OperatorSettingChange
{
public:
    FloatingRateChartSettingChange(Display& display, bool visible, bool (*persist)(bool)) noexcept
        : display_(display), previous_(display.IsChartVisible()), selected_(visible), persist_(persist) {}
    bool ApplyLive() override { return display_.SetChartVisible(selected_); }
    bool Persist() override { return persist_(selected_); }
    bool RollbackLive() override { return display_.SetChartVisible(previous_); }
private:
    Display& display_;
    bool previous_, selected_;
    bool (*persist_)(bool);
};
