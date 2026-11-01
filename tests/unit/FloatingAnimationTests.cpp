#include "floating/FloatingRateRenderer.h"
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

TEST_CASE("Visibility uses cubic easing and reverses without jumping", "[unit][animation]")
{
    VisibilityTransition transition;
    transition.Begin(false, 1000);
    transition.Advance(1110);
    CHECK(transition.progress == Catch::Approx(0.125f));
    const auto midway = transition.progress;
    transition.Begin(true, 1110);
    CHECK(transition.progress == midway);
    transition.Advance(1400);
    CHECK(transition.progress == 1);
    CHECK(transition.duration == 0);
    transition.Begin(false, 1400);
    transition.Advance(1620);
    CHECK(transition.progress == 0);
    CHECK(transition.duration == 0);
}

TEST_CASE("Hidden windows and disabled animations finish at static endpoints", "[unit][animation]")
{
    for (const bool windowVisible : {false, true})
        for (const bool animations : {false, true})
        {
            CAPTURE(windowVisible, animations);
            FloatingRateRenderer renderer;
            renderer.BeginVisibility(true, false, 1000, windowVisible, animations);
            renderer.BeginVisibility(false, false, 1000, windowVisible, animations);
            CHECK_FALSE(renderer.StepAnimation(1300, windowVisible, animations));
            CHECK_FALSE(renderer.IsTextVisible());
            CHECK_FALSE(renderer.IsChartVisible());
            CHECK_FALSE(renderer.HasActiveVisibilityTransition());
            CHECK(renderer.CurrentSizeForDpi(96).cx == 54);
            CHECK_FALSE(renderer.SyncAnimationState(windowVisible, animations));
        }
}

TEST_CASE("Animation state stops for hidden disabled and settled idle displays", "[unit][animation]")
{
    FloatingRateRenderer renderer;
    CHECK_FALSE(renderer.SyncAnimationState(true, true));
    renderer.SetRates(L"1.0 K/s", L"1.0 K/s", 1000, 1000, 1000);
    CHECK(renderer.StepAnimation(1100, true, true));
    CHECK_FALSE(renderer.SyncAnimationState(false, true));
    CHECK_FALSE(renderer.SyncAnimationState(true, false));
    for (unsigned i = 0; i < 40; ++i)
        renderer.SetRates(L"0.0 K/s", L"0.0 K/s", 0, 0, 2000 + i * 1000);
    bool active = true;
    for (ULONGLONG now = 42100; active && now < 45000; now += 100)
        active = renderer.StepAnimation(now, true, true);
    CHECK_FALSE(active);
    CHECK(renderer.TargetWaterLevel() == 0);
    CHECK_FALSE(renderer.HasScrollingTrend());
}

TEST_CASE("Visibility cycles are reversible and failed steps retain direction", "[unit][animation]")
{
    FloatingRateRenderer renderer;
    renderer.BeginVisibility(true, false, 1000, false, true);
    renderer.BeginVisibility(false, false, 1000, false, true);
    for (int cycle = 0; cycle < 2; ++cycle)
        for (const auto expected : {std::pair{false, true}, {true, true}, {true, false}, {false, false}})
        {
            auto step = renderer.NextVisibilityCycleStep();
            CHECK(step.changeText == expected.first);
            CHECK(step.visible == expected.second);
            renderer.CommitVisibilityCycleStep(false, step.visible, step.previousDirection);
            CHECK(renderer.NextVisibilityCycleStep().changeText == step.changeText);
            CHECK(renderer.NextVisibilityCycleStep().visible == step.visible);
            renderer.BeginVisibility(step.changeText, step.visible, 1100, false, true);
            renderer.CommitVisibilityCycleStep(true, step.visible, step.previousDirection);
        }
    renderer.BeginVisibility(true, true, 1200, false, true);
    CHECK(renderer.NextVisibilityCycleStep().changeText);
    CHECK_FALSE(renderer.NextVisibilityCycleStep().visible);
}

TEST_CASE("A failed visibility change can restore the exact in-flight state", "[unit][animation]")
{
    FloatingRateRenderer renderer;
    renderer.BeginVisibility(true, false, 1000, true, true);
    CHECK(renderer.StepAnimation(1110, true, true));
    const auto before = renderer.CaptureVisibilityState();
    const auto width = renderer.CurrentSizeForDpi(144).cx;
    renderer.BeginVisibility(true, true, 1110, true, true);
    renderer.RestoreVisibilityState(before);
    CHECK_FALSE(renderer.IsTextVisible());
    CHECK(renderer.CurrentSizeForDpi(144).cx == width);
    CHECK(renderer.CaptureVisibilityState().text.tick == before.text.tick);
    CHECK(renderer.CaptureVisibilityState().text.duration == before.text.duration);
    CHECK_FALSE(renderer.StepAnimation(1300, true, true));
    CHECK_FALSE(renderer.HasActiveVisibilityTransition());
}
