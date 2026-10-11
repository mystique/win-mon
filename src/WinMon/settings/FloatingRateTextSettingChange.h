// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include "WinMonCore.h"
template<class Display>
class FloatingRateTextSettingChange final : public winmon::OperatorSettingChange
{
public:
    FloatingRateTextSettingChange(Display& display, bool visible, bool (*persist)(bool)) noexcept
        : display_(display), previous_(display.IsTextVisible()), selected_(visible), persist_(persist) {}
    bool ApplyLive() override { return display_.SetTextVisible(selected_); }
    bool Persist() override { return persist_(selected_); }
    bool RollbackLive() override { return display_.SetTextVisible(previous_); }
private:
    Display& display_;
    bool previous_, selected_;
    bool (*persist_)(bool);
};
