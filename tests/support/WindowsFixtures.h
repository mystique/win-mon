#pragma once

#include <afxwin.h>
#include <catch2/catch_test_macros.hpp>
#include <functional>

namespace test
{
class DibSurface final
{
public:
    explicit DibSurface(SIZE size)
    {
        dc = CreateCompatibleDC(nullptr);
        REQUIRE(dc != nullptr);
        BITMAPINFO info{};
        info.bmiHeader = {sizeof(BITMAPINFOHEADER), size.cx, -size.cy, 1, 32, BI_RGB};
        void* bits = nullptr;
        bitmap_ = CreateDIBSection(dc, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
        if (!bitmap_ || !bits)
        {
            DeleteDC(dc);
            FAIL("Create test DIB");
        }
        previous_ = SelectObject(dc, bitmap_);
        if (!previous_ || previous_ == HGDI_ERROR)
        {
            DeleteObject(bitmap_);
            DeleteDC(dc);
            FAIL("Select test DIB");
        }
        pixels = static_cast<DWORD*>(bits);
    }
    ~DibSurface()
    {
        SelectObject(dc, previous_);
        DeleteObject(bitmap_);
        DeleteDC(dc);
    }
    DibSurface(const DibSurface&) = delete;
    DibSurface& operator=(const DibSurface&) = delete;
    HDC dc = nullptr;
    DWORD* pixels = nullptr;

private:
    HBITMAP bitmap_ = nullptr;
    HGDIOBJ previous_ = nullptr;
};

inline void DispatchMessages()
{
    MSG message{};
    while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
    {
        TranslateMessage(&message);
        DispatchMessageW(&message);
    }
}

inline bool PumpUntil(const std::function<bool()>& condition, unsigned timeout = 2000)
{
    const auto deadline = GetTickCount64() + timeout;
    do
    {
        DispatchMessages();
        if (condition())
            return true;
        MsgWaitForMultipleObjects(0, nullptr, FALSE, 8, QS_ALLINPUT);
    } while (GetTickCount64() < deadline);
    return false;
}

inline RECT WindowBounds(HWND window)
{
    RECT rect{};
    REQUIRE(::GetWindowRect(window, &rect));
    return rect;
}
inline LONG Width(HWND window)
{
    const auto rect = WindowBounds(window);
    return rect.right - rect.left;
}

struct CursorRestore final
{
    POINT previous{};
    bool saved = GetCursorPos(&previous) != FALSE;
    ~CursorRestore()
    {
        if (saved)
            SetCursorPos(previous.x, previous.y);
    }
};
} // namespace test
