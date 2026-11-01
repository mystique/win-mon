#include <afxwin.h>
#include <catch2/catch_session.hpp>

CWinApp testApp;

int main(int argc, char** argv)
{
    if (!AfxWinInit(GetModuleHandleW(nullptr), nullptr, GetCommandLineW(), 0))
        return 1;
    SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    return Catch::Session().run(argc, argv);
}
