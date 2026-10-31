# ============================================================================
# slint-windows-mica-template (C++ 移植版 / C++ port)
#
# Slint + Win32/DWM：无边框窗口、Mica 材质、CBT Hook 防闪烁、Snap Layouts。
# Slint + Win32/DWM: borderless windows, Mica backdrop, CBT Hook anti-flicker,
# Win11 Snap Layouts.
# ============================================================================
cmake_minimum_required(VERSION 3.21)

project(slint-windows-mica-template LANGUAGES CXX VERSION 0.1.0)

if(NOT WIN32)
    message(FATAL_ERROR "本项目仅支持 Windows / This project targets Windows only.")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# ----------------------------------------------------------------------------
# 1. 获取 Slint C++ SDK（本地 / 自动下载 / 源码构建三选一）
#    Acquire the Slint C++ SDK (local / auto-download / build from source)
# ----------------------------------------------------------------------------
include(cmake/FetchSlintSdk.cmake)

find_package(Slint REQUIRED)
message(STATUS "Slint SDK: ${SLINT_SDK_DIR}")

# ----------------------------------------------------------------------------
# 2. 可执行文件与 .slint 源码
#    Executable and .slint sources
# ----------------------------------------------------------------------------
add_executable(slint-windows-mica-template
    src/main.cpp
    src/sys_info.cpp
    src/platform/attributes.cpp
    src/platform/borderless.cpp
    src/platform/display.cpp
    src/platform/effects.cpp
    src/platform/hook.cpp
    src/windows/app/app.cpp
    src/windows/app/attributes.cpp
    src/windows/app/controls.cpp
    src/windows/custom_terminal/custom_terminal.cpp
    src/windows/custom_terminal/attributes.cpp
    src/windows/custom_terminal/controls.cpp
    src/windows/native_terminal/native_terminal.cpp
    src/windows/native_terminal/attributes.cpp
    src/windows/native_terminal/native_caption.cpp
)

# 由 slint-compiler 编译入口 .slint，生成 app-window.h + 若干 .cpp
# The SAME entry .slint file the Rust version compiles in build.rs.
# 生成的类型放在 ui:: 命名空间下（AppWindow / WindowControls / ...）
# Generated types live in the ui:: namespace (AppWindow / WindowControls / ...)
slint_target_sources(slint-windows-mica-template
    ui/app-window/app-window.slint
    NAMESPACE ui
)

target_include_directories(slint-windows-mica-template PRIVATE src)

target_compile_definitions(slint-windows-mica-template PRIVATE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
    UNICODE
    _UNICODE
)

target_compile_options(slint-windows-mica-template PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:/W3 /utf-8 /permissive- /EHsc>
)

target_link_libraries(slint-windows-mica-template PRIVATE
    Slint::Slint
    user32
    gdi32
    dwmapi
    shcore
    comctl32
    uxtheme
)

# Release 构建去掉控制台（与 Rust 版 windows_subsystem = "windows" 对应）
# Strip the console in Release builds (mirrors windows_subsystem in the Rust version)
if(WIN32)
    target_link_options(slint-windows-mica-template PRIVATE
        $<$<CONFIG:Release>:/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup>
    )
endif()

# 运行时需要 slint_cpp.dll，复制到产物目录
# Copy slint_cpp.dll next to the executable so it can run
add_custom_command(TARGET slint-windows-mica-template POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${SLINT_SDK_DIR}/lib/slint_cpp.dll"
        "$<TARGET_FILE_DIR:slint-windows-mica-template>"
    COMMENT "Copying slint_cpp.dll"
)
