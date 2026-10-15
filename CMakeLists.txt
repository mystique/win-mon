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

file(GLOB WINMONCORE_SOURCES CONFIGURE_DEPENDS
    src/WinMonCore/*.cpp
    src/WinMonCore/*.h
)
add_library(WinMonCore STATIC ${WINMONCORE_SOURCES})
target_include_directories(WinMonCore PUBLIC src/WinMonCore)
set_property(TARGET WinMonCore PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)

file(GLOB_RECURSE WINMON_SOURCES CONFIGURE_DEPENDS
    src/WinMon/*.cpp
    src/WinMon/*.h
    src/WinMon/res/*.ico
    src/WinMon/res/*.rc
)
add_executable(WinMon WIN32 ${WINMON_SOURCES})
target_include_directories(WinMon PRIVATE src/WinMon)
target_link_libraries(WinMon PRIVATE WinMonCore Shell32 Iphlpapi d2d1 dwrite windowscodecs ole32)
set_target_properties(WinMon PROPERTIES
    VS_DPI_AWARE "PerMonitor"
    INTERPROCEDURAL_OPTIMIZATION_RELEASE ON
)

include(CTest)
if(BUILD_TESTING)
    add_executable(LedRateStripTests tests/RateStripLayoutTests/LedRateStripTests.cpp
        src/WinMon/taskbar/RateStrip.cpp src/WinMon/settings/OperatorSettings.cpp)
    target_include_directories(LedRateStripTests PRIVATE src/WinMon)
    target_link_libraries(LedRateStripTests PRIVATE WinMonCore d2d1 ole32 oleaut32)
    add_test(NAME LedRateStripTests COMMAND LedRateStripTests "${CMAKE_BINARY_DIR}/led-strip-evidence")
    add_executable(RateStripWindowTests tests/RateStripLayoutTests/RateStripWindowTests.cpp
        src/WinMon/taskbar/RateStrip.cpp src/WinMon/settings/OperatorSettings.cpp)
    target_include_directories(RateStripWindowTests PRIVATE src/WinMon)
    target_link_libraries(RateStripWindowTests PRIVATE WinMonCore ole32 oleaut32 dwmapi d2d1)
    add_test(NAME RateStripWindowTests COMMAND RateStripWindowTests "${CMAKE_BINARY_DIR}/rate-strip-evidence")
    add_executable(SettingsTests tests/RateStripLayoutTests/SettingsTests.cpp src/WinMon/settings/OperatorSettings.cpp)
    target_include_directories(SettingsTests PRIVATE src/WinMon)
    target_link_libraries(SettingsTests PRIVATE WinMonCore)
    add_test(NAME SettingsTests COMMAND SettingsTests)
    file(GLOB WINMONCORETESTS_SOURCES CONFIGURE_DEPENDS
        tests/WinMonCoreTests/*.cpp
    )
    add_executable(WinMonCoreTests ${WINMONCORETESTS_SOURCES})
    target_link_libraries(WinMonCoreTests PRIVATE WinMonCore)
    set_property(TARGET WinMonCoreTests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    add_test(NAME WinMonCoreTests COMMAND WinMonCoreTests)

    add_executable(FloatingRateRendererTests
        tests/FloatingRateRendererTests/FloatingRateRendererTests.cpp
        src/WinMon/floating/FloatingRateRenderer.cpp)
    target_include_directories(FloatingRateRendererTests PRIVATE src/WinMon)
    target_link_libraries(FloatingRateRendererTests PRIVATE d2d1 dwrite windowscodecs ole32)
    add_test(NAME FloatingRateRendererTests COMMAND FloatingRateRendererTests "${CMAKE_BINARY_DIR}/floating-evidence")


    add_executable(NetworkObservationTests
        tests/NetworkObservationTests/NetworkObservationTests.cpp
        src/WinMon/platform/NetworkObservationReader.cpp)
    target_include_directories(NetworkObservationTests PRIVATE src/WinMon src/WinMonCore)
    target_link_libraries(NetworkObservationTests PRIVATE Iphlpapi ole32)
    add_test(NAME NetworkObservationTests COMMAND NetworkObservationTests)

    add_executable(FloatingWindowTests
        tests/FloatingRateRendererTests/FloatingWindowTests.cpp
        src/WinMon/floating/FloatingRateDisplay.cpp src/WinMon/floating/FloatingRateRenderer.cpp)
    target_include_directories(FloatingWindowTests PRIVATE src/WinMon)
    target_link_libraries(FloatingWindowTests PRIVATE d2d1 dwrite windowscodecs ole32 dwmapi)
    add_test(NAME FloatingWindowTests COMMAND FloatingWindowTests "${CMAKE_BINARY_DIR}/floating-evidence")
endif()

add_custom_command(TARGET WinMon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/licenses" "$<TARGET_FILE_DIR:WinMon>/licenses")
