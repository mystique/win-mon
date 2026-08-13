cmake_minimum_required(VERSION 3.25)

project(WinMon LANGUAGES CXX RC)

if(NOT WIN32 OR NOT MSVC)
    message(FATAL_ERROR "Win Mon requires Windows and the MSVC toolchain")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Win Mon supports x64 builds only")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
set(CMAKE_MFC_FLAG 2)

add_compile_definitions(
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    _WIN32_WINNT=0x0A00
    WINVER=0x0A00
    UNICODE
    _UNICODE
)
add_compile_options(/W4 /sdl /permissive-)

add_library(WinMonCore STATIC
    src/WinMonCore/WinMonCore.cpp
    src/WinMonCore/WinMonCore.h
)
target_include_directories(WinMonCore PUBLIC src/WinMonCore)
set_property(TARGET WinMonCore PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

add_executable(WinMon WIN32
    src/WinMon/Autostart.cpp
    src/WinMon/Autostart.h
    src/WinMon/RateStrip.cpp
    src/WinMon/RateStrip.h
    src/WinMon/Settings.cpp
    src/WinMon/Settings.h
    src/WinMon/TrayMessageWindow.cpp
    src/WinMon/TrayMessageWindow.h
    src/WinMon/WinMonApp.cpp
    src/WinMon/WinMonApp.h
    src/WinMon/res/resource.h
    src/WinMon/res/WinMon.ico
    src/WinMon/res/WinMonTrayDark.ico
    src/WinMon/res/WinMonTrayLight.ico
    src/WinMon/res/WinMon.rc
)
target_link_libraries(WinMon PRIVATE WinMonCore Shell32 Iphlpapi)
set_target_properties(WinMon PROPERTIES
    VS_DPI_AWARE "PerMonitor"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)

include(CTest)
if(BUILD_TESTING)
    add_executable(WinMonCoreTests
        tests/WinMonCoreTests/WinMonCoreTests.cpp
    )
    target_link_libraries(WinMonCoreTests PRIVATE WinMonCore)
    set_property(TARGET WinMonCoreTests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    add_test(NAME WinMonCoreTests COMMAND WinMonCoreTests)
endif()
