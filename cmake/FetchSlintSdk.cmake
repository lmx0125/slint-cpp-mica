# ============================================================================
# 获取 Slint C++ SDK / Acquire the Slint C++ SDK
#
# 查找顺序 / Lookup order:
#   1. -DSLINT_SDK_DIR=<path>                   手动指定
#   2. third_party/slint-sdk/lib/cmake/Slint    随仓库放置的预编译 SDK
#   3. 自动下载官方预编译包并静默解压到 build 目录
#      Auto-download the official prebuilt package into the build dir
#   4. -DSLINT_BUILD_FROM_SOURCE=ON             FetchContent 源码构建（需要 Rust 工具链）
#
# 说明 / Note:
#   官方预编译包 (Slint-cpp-<ver>-win64-MSVC-AMD64.exe) 内含 headers、slint_cpp.dll
#   与 slint-compiler.exe，无需 Rust 即可使用，因此默认走预编译包。
#   The official package ships headers, the DLL and slint-compiler.exe, so no Rust
#   toolchain is needed; that is the default path.
# ============================================================================

set(SLINT_VERSION "1.17.1" CACHE STRING "Slint version to use")

set(SLINT_SDK_DIR "" CACHE PATH "Path to a Slint C++ SDK (contains lib/cmake/Slint)")

if(SLINT_SDK_DIR AND EXISTS "${SLINT_SDK_DIR}/lib/cmake/Slint/SlintConfig.cmake")
    message(STATUS "Using Slint SDK from SLINT_SDK_DIR: ${SLINT_SDK_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${SLINT_SDK_DIR}")
    return()
endif()

set(_slint_vendored "${CMAKE_CURRENT_LIST_DIR}/../third_party/slint-sdk")
if(EXISTS "${_slint_vendored}/lib/cmake/Slint/SlintConfig.cmake")
    set(SLINT_SDK_DIR "${_slint_vendored}" CACHE PATH "" FORCE)
    message(STATUS "Using vendored Slint SDK: ${SLINT_SDK_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${SLINT_SDK_DIR}")
    return()
endif()

option(SLINT_BUILD_FROM_SOURCE "Build Slint from source via FetchContent (requires Rust)" OFF)

if(SLINT_BUILD_FROM_SOURCE)
    message(STATUS "Building Slint ${SLINT_VERSION} from source (requires Rust toolchain)")
    include(FetchContent)
    set(SLINT_FEATURE_BACKEND_WINIT ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_SKIA ON CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_RENDERER_FEMTOVG OFF CACHE BOOL "" FORCE)
    set(SLINT_FEATURE_INTERPRETER OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(slint
        GIT_REPOSITORY https://github.com/slint-ui/slint.git
        GIT_TAG "v${SLINT_VERSION}"
        GIT_SHALLOW TRUE
        SOURCE_SUBDIR api/cpp
    )
    FetchContent_MakeAvailable(slint)
    return()
endif()

# ---------------------------------------------------------------------------
# 自动下载官方预编译 SDK / Auto-download the official prebuilt SDK
# ---------------------------------------------------------------------------
set(_slint_bits 64)
if(CMAKE_SIZEOF_VOID_P EQUAL 4)
    set(_slint_bits 32)
endif()

if(NOT _slint_bits EQUAL 64)
    message(FATAL_ERROR "预编译 SDK 仅提供 x64 版本 / Prebuilt SDK is x64-only. Use -DSLINT_SDK_DIR or -DSLINT_BUILD_FROM_SOURCE=ON")
endif()

set(_slint_sdk_cache "${CMAKE_BINARY_DIR}/_deps/slint-sdk")
if(EXISTS "${_slint_sdk_cache}/lib/cmake/Slint/SlintConfig.cmake")
    set(SLINT_SDK_DIR "${_slint_sdk_cache}" CACHE PATH "" FORCE)
    message(STATUS "Using cached downloaded Slint SDK: ${SLINT_SDK_DIR}")
    list(APPEND CMAKE_PREFIX_PATH "${SLINT_SDK_DIR}")
    return()
endif()

set(_slint_installer "${CMAKE_BINARY_DIR}/_deps/Slint-cpp-${SLINT_VERSION}-win64-MSVC-AMD64.exe")
set(_slint_url "https://github.com/slint-ui/slint/releases/download/v${SLINT_VERSION}/Slint-cpp-${SLINT_VERSION}-win64-MSVC-AMD64.exe")

message(STATUS "Downloading Slint C++ SDK ${SLINT_VERSION} ...")
message(STATUS "  from ${_slint_url}")
file(DOWNLOAD "${_slint_url}" "${_slint_installer}"
    STATUS _slint_download_status
    SHOW_PROGRESS
    TLS_VERIFY ON
)
list(GET _slint_download_status 0 _slint_download_code)
if(NOT _slint_download_code EQUAL 0)
    list(GET _slint_download_status 1 _slint_download_msg)
    message(FATAL_ERROR
        "下载 Slint SDK 失败 / Failed to download the Slint SDK: ${_slint_download_msg}\n"
        "可手动下载后使用 -DSLINT_SDK_DIR=<解压目录> 指定 / "
        "Download it manually and pass -DSLINT_SDK_DIR=<dir>")
endif()

message(STATUS "Extracting Slint C++ SDK ...")
# 安装器为 NSIS 自解压包：/S 静默，/D 指定目录（必须为最后一个参数且不加引号）
# NSIS self-extracting package: /S silent, /D target dir (last arg, unquoted)
execute_process(
    COMMAND "${_slint_installer}" /S "/D=${_slint_sdk_cache}"
    RESULT_VARIABLE _slint_install_result
)
if(NOT _slint_install_result EQUAL 0 OR NOT EXISTS "${_slint_sdk_cache}/lib/cmake/Slint/SlintConfig.cmake")
    message(FATAL_ERROR
        "解压 Slint SDK 失败 / Failed to extract the Slint SDK.\n"
        "请手动运行 ${_slint_installer} 并指定 -DSLINT_SDK_DIR=<解压目录>")
endif()

# 解压后清理安装器与卸载器，避免污染 build 目录
# Clean up the installer and uninstaller after extraction
file(REMOVE "${_slint_sdk_cache}/Uninstall.exe")
file(REMOVE "${_slint_installer}")

set(SLINT_SDK_DIR "${_slint_sdk_cache}" CACHE PATH "" FORCE)
list(APPEND CMAKE_PREFIX_PATH "${SLINT_SDK_DIR}")
message(STATUS "Slint SDK ready: ${SLINT_SDK_DIR}")
