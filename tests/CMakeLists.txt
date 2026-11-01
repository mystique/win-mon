include(FetchContent)
FetchContent_Declare(Catch2
    URL https://codeload.github.com/catchorg/Catch2/tar.gz/refs/tags/v3.16.1
    URL_HASH SHA256=d16bd1b8b2364918bd472cb216394acb93bde6784890df0d91bccfb3f28d777d
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(Catch2)
list(APPEND CMAKE_MODULE_PATH "${catch2_SOURCE_DIR}/extras")
include(Catch)

# Test the same production sources; no alternate implementation or test-only defines.
add_library(WinMonTestNetwork STATIC ../src/WinMon/platform/NetworkObservationReader.cpp)
target_include_directories(WinMonTestNetwork PUBLIC ../src/WinMon)
target_link_libraries(WinMonTestNetwork PUBLIC WinMonCore Iphlpapi ole32)

add_library(WinMonTestRendering STATIC ../src/WinMon/floating/FloatingRateRenderer.cpp)
target_include_directories(WinMonTestRendering PUBLIC ../src/WinMon)
target_link_libraries(WinMonTestRendering PUBLIC d2d1 windowscodecs ole32)

add_library(WinMonTestWindows STATIC
    ../src/WinMon/platform/LaunchAtLogin.cpp
    ../src/WinMon/settings/OperatorSettings.cpp
    ../src/WinMon/floating/FloatingRateDisplay.cpp
    ../src/WinMon/taskbar/RateStrip.cpp
    ../src/WinMon/app/TrayMessageWindow.cpp
)
target_include_directories(WinMonTestWindows PUBLIC ../src/WinMon)
target_link_libraries(WinMonTestWindows PUBLIC WinMonCore WinMonTestNetwork WinMonTestRendering
    Shell32 Iphlpapi d2d1 dwrite windowscodecs ole32 oleaut32 Advapi32)

add_executable(WinMonUnitTests
    support/UnitMain.cpp
    unit/NetworkSessionTests.cpp
    unit/OperatorMenuTests.cpp
    unit/SettingTransactionTests.cpp
    unit/ShellLifecycleTests.cpp
    unit/NetworkObservationTests.cpp
    unit/LedReadoutTests.cpp
    unit/FloatingAnimationTests.cpp
)
target_include_directories(WinMonUnitTests PRIVATE .)
target_link_libraries(WinMonUnitTests PRIVATE Catch2::Catch2 WinMonCore WinMonTestNetwork WinMonTestRendering)
set_property(TARGET WinMonUnitTests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
catch_discover_tests(WinMonUnitTests TEST_PREFIX "unit/" PROPERTIES LABELS unit TIMEOUT 15)

add_executable(WinMonIntegrationTests
    support/WindowsMain.cpp
    integration/SettingsTests.cpp
    integration/LaunchAtLoginTests.cpp
    integration/FloatingRendererTests.cpp
    integration/RateStripRendererTests.cpp
    integration/FloatingWindowTests.cpp
    integration/HostLifecycleTests.cpp
)
target_include_directories(WinMonIntegrationTests PRIVATE .)
target_link_libraries(WinMonIntegrationTests PRIVATE Catch2::Catch2 WinMonTestWindows)
set_property(TARGET WinMonIntegrationTests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
catch_discover_tests(WinMonIntegrationTests TEST_PREFIX "integration/"
    PROPERTIES LABELS integration TIMEOUT 30 RESOURCE_LOCK winmon_desktop)

option(WINMON_INTERACTIVE_TESTS "Register tests that require an interactive Windows 11 desktop" OFF)
if(WINMON_INTERACTIVE_TESTS)
    add_executable(WinMonAcceptanceTests support/WindowsMain.cpp
        acceptance/TaskbarTests.cpp acceptance/OperatorActionTests.cpp ../src/WinMon/res/WinMon.rc)
    target_include_directories(WinMonAcceptanceTests PRIVATE . ../src/WinMon/res)
    target_link_libraries(WinMonAcceptanceTests PRIVATE Catch2::Catch2 WinMonTestWindows)
    set_property(TARGET WinMonAcceptanceTests PROPERTY INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
    catch_discover_tests(WinMonAcceptanceTests TEST_PREFIX "acceptance/"
        PROPERTIES LABELS acceptance TIMEOUT 30 RESOURCE_LOCK winmon_desktop SKIP_REGULAR_EXPRESSION "SKIPPED")
    find_program(WINMON_POWERSHELL NAMES pwsh REQUIRED)
    add_test(NAME acceptance/ApplicationLifecycle COMMAND "${WINMON_POWERSHELL}"
        -NoProfile -File "${CMAKE_CURRENT_SOURCE_DIR}/acceptance/windows-smoke.ps1"
        -Executable "$<TARGET_FILE:WinMon>")
    set_tests_properties(acceptance/ApplicationLifecycle PROPERTIES
        LABELS acceptance TIMEOUT 60 RESOURCE_LOCK winmon_desktop)
endif()

add_executable(WinMonVisualEvidence EXCLUDE_FROM_ALL visual/VisualEvidence.cpp)
target_link_libraries(WinMonVisualEvidence PRIVATE WinMonTestRendering)
add_custom_target(visual-evidence
    COMMAND WinMonVisualEvidence "${CMAKE_BINARY_DIR}/visual-evidence"
    DEPENDS WinMonVisualEvidence
    COMMENT "Generate visual evidence for manual review (not an automated test)")
