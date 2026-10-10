#pragma once
#include "../WinMonCore/WinMonCore.h"

template<class Strip>
class SpeedTextPositionSettingChange final : public winmon::OperatorSettingChange
{
public:
    SpeedTextPositionSettingChange(Strip& strip, bool left, bool (*persist)(bool)) noexcept
        : strip_(strip), previous_(strip.IsSpeedTextOnLeft()), selected_(left), persist_(persist) {}
    bool ApplyLive() override { return strip_.SetSpeedTextOnLeft(selected_); }
    bool Persist() override { return persist_(selected_); }
    bool RollbackLive() override { return strip_.SetSpeedTextOnLeft(previous_); }
private:
    Strip& strip_;
    bool previous_, selected_;
    bool (*persist_)(bool);
};
