#pragma once
#include <array>
#include <cstdint>
// Copyright 2026 The Punctum Project Authors. Derived subset under SIL OFL 1.1.
// Source: sundyme/punctum, 56caad6cb7295c98fede26edb196d763e124b75d/src/glyphs.py
// Full license: licenses/Punctum-OFL.txt (distributed beside WinMon.exe).
namespace punctum
{
struct Glyph { wchar_t character; std::array<std::uint8_t, 7> rows; };
inline constexpr std::array<Glyph, 18> glyphs{{
    {L'\u2191', {4, 14, 21, 4, 4, 4, 4}},
    {L'\u2193', {4, 4, 4, 4, 21, 14, 4}},
    {L'0', {14, 17, 25, 21, 19, 17, 14}},
    {L'1', {4, 6, 4, 4, 4, 4, 14}},
    {L'2', {14, 17, 16, 8, 4, 2, 31}},
    {L'3', {14, 17, 16, 12, 16, 17, 14}},
    {L'4', {8, 12, 10, 9, 31, 8, 8}},
    {L'5', {31, 1, 15, 16, 16, 17, 14}},
    {L'6', {12, 2, 1, 15, 17, 17, 14}},
    {L'7', {31, 16, 8, 4, 4, 4, 4}},
    {L'8', {14, 17, 17, 14, 17, 17, 14}},
    {L'9', {14, 17, 17, 30, 16, 8, 6}},
    {L'.', {0, 0, 0, 0, 0, 0, 4}},
    {L'K', {17, 9, 5, 3, 5, 9, 17}},
    {L'M', {17, 27, 21, 21, 17, 17, 17}},
    {L'G', {14, 17, 1, 29, 17, 17, 14}},
    {L'/', {16, 8, 8, 4, 2, 2, 1}},
    {L's', {0, 0, 30, 1, 14, 16, 15}},
}};
inline const Glyph* Find(wchar_t character) noexcept
{
    for (const auto& glyph : glyphs) if (glyph.character == character) return &glyph;
    return nullptr;
}
}
