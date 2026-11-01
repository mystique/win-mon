#include "floating/LedReadout.h"
#include <catch2/catch_test_macros.hpp>

TEST_CASE("LED text parsing rejects malformed input and saturates overflow", "[unit][readout]")
{
    std::array<wchar_t, 9> cells{}, saturated{};
    REQUIRE(led::Cells(L"999.9 G/s", L'\u2191', saturated));
    REQUIRE((led::Cells(L"1000.0 G/s", L'\u2191', cells) && cells == saturated));
    REQUIRE((led::Cells(L"0000.0 K/s", L'\u2191', cells) && cells[1] == L' ' && cells[3] == L'0'));
    REQUIRE((!led::Cells(L"12.x K/s", L'\u2191', cells) && !led::Cells(L"12.0 B/s", L'\u2191', cells)));
}
