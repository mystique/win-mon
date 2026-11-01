#include "support/CoreFixtures.h"

using namespace winmon;
using namespace test;

class FakeShellSurface final : public ShellSurface
{
public:
    bool EnsureTrayIcon() override
    {
        ++ensureTrayCalls;
        return trayResult;
    }
    void ForgetTrayIcon() noexcept override
    {
        ++forgetTrayCalls;
    }
    RateStripState RecreateRateStrip() override
    {
        ++recreateCalls;
        return recreateResult;
    }
    RateStripState RefreshRateStrip() override
    {
        ++refreshCalls;
        return refreshResult;
    }
    void DestroyRateStrip() noexcept override
    {
        ++destroyCalls;
    }
    void RemoveTrayIcon() noexcept override
    {
        ++removeTrayCalls;
    }
    void ScheduleRecovery() noexcept override
    {
        recoveryScheduled = true;
    }
    void CancelRecovery() noexcept override
    {
        recoveryScheduled = false;
    }

    bool trayResult = true;
    RateStripState recreateResult = RateStripState::Visible;
    RateStripState refreshResult = RateStripState::Visible;
    int ensureTrayCalls = 0;
    int forgetTrayCalls = 0;
    int recreateCalls = 0;
    int refreshCalls = 0;
    int destroyCalls = 0;
    int removeTrayCalls = 0;
    bool recoveryScheduled = false;
};

TEST_CASE("Shell lifecycle recovers surfaces and respects visibility", "[unit][shell]")
{
    FakeShellSurface surface;
    ShellLifecycle lifecycle(surface);
    REQUIRE(lifecycle.Start());
    REQUIRE((surface.ensureTrayCalls == 1 && surface.recreateCalls == 1));

    surface.refreshResult = RateStripState::Retry;
    lifecycle.OnRateSample();
    REQUIRE((surface.refreshCalls == 1 && surface.recoveryScheduled));

    surface.recreateResult = RateStripState::Hidden;
    lifecycle.Retry();
    REQUIRE(!surface.recoveryScheduled);

    surface.recreateResult = RateStripState::Visible;
    lifecycle.OnShellCreated();
    REQUIRE((surface.forgetTrayCalls == 1 && surface.ensureTrayCalls == 3));
    REQUIRE((surface.recreateCalls == 3 && !surface.recoveryScheduled));

    lifecycle.OnEnvironmentChanged();
    REQUIRE(surface.recreateCalls == 4);

    lifecycle.Shutdown();
    REQUIRE((surface.destroyCalls == 1 && surface.removeTrayCalls == 1 && !surface.recoveryScheduled));
    lifecycle.Retry();
    REQUIRE(surface.ensureTrayCalls == 3);
}

TEST_CASE("Initial tray creation failure aborts startup", "[unit][shell]")
{
    FakeShellSurface surface;
    surface.trayResult = false;
    ShellLifecycle lifecycle(surface);
    REQUIRE(!lifecycle.Start());
    REQUIRE((surface.recreateCalls == 0 && !surface.recoveryScheduled));
}

TEST_CASE("Shell recovery retries a missing tray before rebuilding the strip", "[unit][shell]")
{
    FakeShellSurface surface;
    ShellLifecycle lifecycle(surface);
    REQUIRE(lifecycle.Start());
    surface.trayResult = false;
    lifecycle.OnShellCreated();
    CHECK(surface.forgetTrayCalls == 1);
    CHECK(surface.recreateCalls == 1);
    CHECK(surface.recoveryScheduled);
    lifecycle.Retry();
    CHECK(surface.recreateCalls == 1);
    CHECK(surface.recoveryScheduled);
    surface.trayResult = true;
    lifecycle.Retry();
    CHECK(surface.recreateCalls == 2);
    CHECK_FALSE(surface.recoveryScheduled);
}

TEST_CASE("Shutdown is idempotent and ignores every queued shell event", "[unit][shell]")
{
    FakeShellSurface surface;
    ShellLifecycle lifecycle(surface);
    REQUIRE(lifecycle.Start());
    lifecycle.Shutdown();
    lifecycle.Shutdown();
    lifecycle.OnShellCreated();
    lifecycle.OnEnvironmentChanged();
    lifecycle.OnRateSample();
    lifecycle.Retry();
    CHECK(surface.ensureTrayCalls == 1);
    CHECK(surface.recreateCalls == 1);
    CHECK(surface.refreshCalls == 0);
    CHECK(surface.destroyCalls == 1);
    CHECK(surface.removeTrayCalls == 1);
    CHECK_FALSE(surface.recoveryScheduled);
}
