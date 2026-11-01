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
    add_subdirectory(tests)
endif()

add_custom_command(TARGET WinMon POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_SOURCE_DIR}/licenses" "$<TARGET_FILE_DIR:WinMon>/licenses")
