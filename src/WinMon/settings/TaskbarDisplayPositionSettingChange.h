// SPDX-License-Identifier: MIT
// Copyright (c) 2026 mk2.app

#pragma once
#include "WinMonCore.h"

template<class Strip>
class TaskbarDisplayPositionSettingChange final : public winmon::OperatorSettingChange
{
public:
    TaskbarDisplayPositionSettingChange(Strip& strip, bool left, bool (*persist)(bool)) noexcept
        : strip_(strip), previous_(strip.IsTaskbarDisplayOnLeft()), selected_(left), persist_(persist) {}
    bool ApplyLive() override { return strip_.SetTaskbarDisplayOnLeft(selected_); }
    bool Persist() override { return persist_(selected_); }
    bool RollbackLive() override { return strip_.SetTaskbarDisplayOnLeft(previous_); }
private:
    Strip& strip_;
    bool previous_, selected_;
    bool (*persist_)(bool);
};
