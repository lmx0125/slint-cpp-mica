#include "windows/native_terminal/native_terminal.h"

#include <chrono>
#include <cstdio>

#include "app-window.h"
#include "platform/display.h"
#include "platform/hook.h"
#include "sys_info.h"
#include "windows/native_terminal/attributes.h"
#include "windows/native_terminal/native_caption.h"

namespace windows::native_terminal {

namespace {

/// 原生终端窗口标题（须与 .slint 中的 `title` 一致，用于 CBT Hook 匹配）
/// Native terminal title (must match the `title` property in the .slint file)
constexpr const wchar_t* kNativeTerminalTitle = L"原生终端日志 (Console Output)";

} // namespace

bool open(const WindowHolderPtr& handle, CloseBehavior close_behavior)
{
    // Hide 模式下若实例已存在，直接重新显示
    // In Hide mode, re-show the existing instance when present
    if (close_behavior == CloseBehavior::Hide && handle->component.has_value()) {
        (*handle->component)->window().show();
        std::printf("[NativeTerminal] 复用已有窗口实例并重新显示 (Hide 模式) / Reusing the "
                    "existing instance (Hide mode)\n");
        return true;
    }

    auto frame = std::make_shared<NativeCaptionFrame>();

    // ── 1. CBT Hook：在 CreateWindowExW 瞬间完成 DWM 属性 + 原生边框子类化，
    //        全部在窗口可见之前完成，因此不会出现边框闪烁 ──
    // ── 1. CBT Hook: apply DWM attributes and the native frame subclassing at
    //        CreateWindowExW time, i.e. while the window is still hidden ──
    auto hook = platform::CbtHookGuard::install(
        std::wstring(kNativeTerminalTitle),
        [frame](HWND hwnd) {
            // 此时 HWND 已创建但尚未显示 / The HWND exists but is not visible yet
            attributes::get_attributes().apply(hwnd);
            if (frame->apply_to_hwnd(hwnd)) {
                std::printf("[NativeTerminal] 🎯 CBT Hook 已在隐藏状态完成原生边框配置 / "
                            "Native frame configured while hidden\n");
            }
        });

    const bool hook_ok = hook.has_value();
    if (hook_ok) {
        std::printf("[NativeTerminal] CBT Hook 已安装，等待 show() 捕获窗口 / CBT Hook "
                    "installed, waiting for show()\n");
        hook->detach();
    } else {
        std::printf("[NativeTerminal] CBT Hook 安装失败，将以 fallback 方式运行 / CBT Hook "
                    "installation failed, running in fallback mode\n");
    }

    // ── 2. 创建组件（保持隐藏）/ Create the component (stays hidden) ──
    auto terminal = ui::NativeTerminalWindow::create();

    // ── 3. 显式设置初始尺寸与位置，供 Slint 创建窗口时使用 ──
    // ── 3. Set the geometry explicitly before Slint creates the window ──
    terminal->window().set_size(slint::LogicalSize{
        slint::Size<float>{terminal->get_init_width(), terminal->get_init_height()}});
    platform::center_on_active_monitor(
        terminal->window(), terminal->get_init_width(), terminal->get_init_height());

    terminal->set_is_mica_active(sys_info::is_win11());

    // ── 4. 绑定 UI 回调 / Bind the UI callbacks ──
    const slint::ComponentWeakHandle<ui::NativeTerminalWindow> weak(terminal);

    // 4a. 激活状态 → 标题栏失焦变淡（与主窗口、自绘终端的表现一致）
    // 4a. Activation state → titlebar inactive fade (consistent with the other windows)
    frame->on_active_changed([weak](bool is_active) {
        if (auto strong = weak.lock()) {
            if ((*strong)->get_titlebar_is_active() != is_active) {
                (*strong)->set_titlebar_is_active(is_active);
            }
        }
    });
    const auto handle_for_close = handle;
    terminal->on_close_requested([weak, handle_for_close, close_behavior] {
        if (auto strong = weak.lock()) {
            (*strong)->window().hide();
        }
        if (close_behavior == CloseBehavior::Hide) {
            std::printf("[NativeTerminal] 窗口已隐藏 (Hide) / Window hidden (Hide)\n");
        } else {
            handle_for_close->component.reset();
            std::printf("[NativeTerminal] 窗口已关闭并释放资源 (Destroy) / Window closed and "
                        "released (Destroy)\n");
        }
    });

    // 4b. 标题栏拖拽（Slint 拖拽区 → Win32 原生拖拽）
    // 4b. Titlebar drag (Slint drag area → native Win32 drag)
    terminal->on_drag([frame] { frame->drag(); });

    // 4c. 双击标题栏切换最大化 / 还原
    // 4c. Double-click the titlebar to toggle maximize / restore
    terminal->on_double_click([frame] { frame->toggle_maximized(); });

    // 4d. 复制日志 / Copy log
    terminal->on_copy_log([weak] {
        if (weak.lock()) {
            std::printf("[NativeTerminal] 复制日志（功能预留） / Copy log (feature reserved)\n");
        }
    });

    // ── 5. 显示窗口（若 Hook 未生效，则退化为事件循环中的补丁安装） ──
    // ── 5. Show the window (fall back to event-loop patching when the hook missed) ──
    terminal->window().show();

    if (!hook_ok) {
        slint::Timer::single_shot(std::chrono::milliseconds(0), [frame, weak]() mutable {
            if (frame->hwnd() != nullptr) {
                return;
            }
            if (auto strong = weak.lock()) {
                HWND hwnd = (*strong)->window().win32_hwnd();
                if (hwnd != nullptr) {
                    attributes::get_attributes().apply(hwnd);
                    frame->apply_to_hwnd(hwnd);
                }
            }
        });
    }

    handle->component = std::move(terminal);
    return true;
}

} // namespace windows::native_terminal
