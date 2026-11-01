#include "WinMonCore.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/generators/catch_generators.hpp>
#include <string>

using namespace winmon;

TEST_CASE("Setting transactions preserve state and call recovery in order", "[unit][transaction]")
{
    const bool apply = GENERATE(false, true);
    const bool persist = GENERATE(false, true);
    const bool rollback = GENERATE(false, true);
    CAPTURE(apply, persist, rollback);
    int firstDisplay = 1, secondDisplay = 1, saved = 1;
    std::string calls;
    const auto outcome = OperatorSettingTransaction::Commit(
        [&] {
            calls += 'A';
            firstDisplay = 2;
            if (apply)
                secondDisplay = 2;
            return apply;
        },
        [&] {
            calls += 'P';
            if (persist)
                saved = 2;
            return persist;
        },
        [&] {
            calls += 'R';
            if (rollback)
                firstDisplay = secondDisplay = 1;
            return rollback;
        });
    if (apply && persist)
    {
        CHECK(outcome == OperatorSettingOutcome::Applied);
        CHECK(calls == "AP");
        CHECK(firstDisplay == 2);
        CHECK(secondDisplay == 2);
        CHECK(saved == 2);
    }
    else
    {
        CHECK(calls == (apply ? "APR" : "AR"));
        CHECK(saved == 1);
        if (rollback)
        {
            CHECK(outcome ==
                  (apply ? OperatorSettingOutcome::RolledBack : OperatorSettingOutcome::LiveApplicationFailed));
            CHECK(firstDisplay == 1);
            CHECK(secondDisplay == 1);
        }
        else
            CHECK(outcome == OperatorSettingOutcome::RecoveryRequired);
    }
}
