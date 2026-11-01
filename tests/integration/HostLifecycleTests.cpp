#include "app/TrayMessageWindow.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("The message host can shut down before initialization and more than once", "[integration][lifecycle]")
{
    TrayMessageWindow host;
    host.Shutdown();
    host.Shutdown();
    CHECK(host.GetSafeHwnd() == nullptr);
}
