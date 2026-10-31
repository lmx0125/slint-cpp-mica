#include "windows/custom_terminal/custom_terminal.h"

#include <chrono>
#include <string>

#include "app-window.h"
#include "platform/borderless.h"
#include "platform/controls.h"
#include "platform/display.h"
#include "platform/effects.h"
#include "platform/hook.h"
#include "windows/custom_terminal/attributes.h"
#include "windows/custom_terminal/controls.h"

namespace windows::custom_terminal {

bool open(const WindowHolderPtr& handle, CloseBehavior close_behavior)
{
    // 共享 Frame 持有器：CBT Hook 回调需要 WindowFrame 来安装子类化。
    // CBT Hook 触发时已有 HWND，但 Slint 侧的 winit 窗口对象尚未就绪，
    // 因此走 apply_to_hwnd() 而不是 apply()。
    // Shared frame holder: the CBT Hook callback needs the WindowFrame to install the
    // subclassing. At hook time the HWND exists but Slint's window object is not created
    // yet, so apply_to_hwnd() is used instead of apply().
    auto frame_holder = std::make_shared<std::optional<platform::WindowFrame>>();

    // Hide 模式下若实例已存在，直接重新显示
    // In Hide mode, re-show the existing instance when present
    if (close_behavior == CloseBehavior::Hide && handle->component.has_value()) {
        (*handle->component)->window().show();
        std::printf("[CustomTerminal] 复用已有窗口实例并重新显示 (Hide 模式) / Reusing the "
                    "existing instance (Hide mode)\n");
        return true;
    }

    // ── 1. 安装 CBT Hook（按标题匹配，在 CreateWindowExW 瞬间注入属性 + 子类化） ──
    // ── 1. Install the CBT Hook (title-matched; injects attributes + subclassing at
    //       CreateWindowExW) ──
    auto hook = platform::CbtHookGuard::install(
        std::wstring(kTerminalTitle),
        [frame_holder](HWND hwnd) {
            auto attributes = attributes::get_attributes();
            attributes.apply(hwnd);
            std::printf("[CustomTerminal] 🎯 CBT Hook 成功捕获窗口并注入 DWM 属性 / Captured the "
                        "window and injected DWM attributes\n");

            if (frame_holder->has_value()) {
                (*frame_holder)->apply_to_hwnd(hwnd);
                std::printf("[CustomTerminal] 🎯 CBT Hook 已通过 HWND 安装 Win32 子类化 / "
                            "Installed the Win32 subclassing\n");
            } else {
                std::printf("[CustomTerminal] ⚠️ CBT Hook 回调时 frame 尚未就绪 / The frame was "
                            "not ready in the CBT callback\n");
            }
        });

    const bool hook_ok = hook.has_value();
    if (hook.has_value()) {
        std::printf("[CustomTerminal] CBT Hook 已安装，等待 show() 捕获窗口 / CBT Hook installed, "
                    "waiting for show() to capture the window\n");
        // Hook 会在 HCBT_ACTIVATE 后自卸载，这里放弃所有权
        // The hook unhooks itself after HCBT_ACTIVATE, so drop ownership here
        hook->detach();
    } else {
        std::printf("[CustomTerminal] CBT Hook 安装失败，将以 fallback 方式运行 / CBT Hook "
                    "installation failed, running in fallback mode\n");
    }

    // ── 2. 创建组件实例 / Create the component instance ──
    auto terminal = ui::CustomTerminalWindow::create();

    // ── 3. 创建无边框框架（子类化由 CBT Hook 在 HCBT_ACTIVATE 时安装） ──
    // ── 3. Create the borderless frame (installed by the hook on HCBT_ACTIVATE) ──
    platform::TitlebarButtons buttons;
    const slint::ComponentWeakHandle<ui::CustomTerminalWindow> weak(terminal);
    auto adapter = std::make_shared<TerminalTitlebarAdapter>(weak, buttons);
    platform::WindowFrame frame(platform::make_window_accessor(weak), adapter);
    *frame_holder = frame;

    // ── 4. 活动屏幕居中定位与防超屏 / Center on the active monitor ──
    platform::center_on_active_monitor(
        terminal->window(), terminal->get_init_width(), terminal->get_init_height());

    // ── 5. Mica 特效联动 / Mica visual effect ──
    platform::apply_mica_effect([&terminal] { terminal->set_is_mica_active(true); }, hook_ok);

    // ── 6. 绑定 UI 回调 / Bind the UI callbacks ──
    // 6a. 关闭：Destroy → hide() 并清空持有器；Hide → 只 hide()
    // 6a. Close: Destroy → hide() and clear the holder; Hide → hide() only
    const auto handle_for_close = handle;
    terminal->on_close_requested([weak, handle_for_close, close_behavior] {
        if (auto strong = weak.lock()) {
            (*strong)->window().hide();
        }
        if (close_behavior == CloseBehavior::Hide) {
            std::printf("[CustomTerminal] 窗口已隐藏 (Hide) / Window hidden (Hide)\n");
        } else {
            handle_for_close->component.reset();
            std::printf("[CustomTerminal] 窗口已关闭并释放资源 (Destroy) / Window closed and "
                        "released (Destroy)\n");
        }
    });

    // 6b. 标题栏拖拽 / Titlebar drag
    platform::WindowFrame frame_drag = frame;
    terminal->on_drag([frame_drag]() mutable { frame_drag.drag(); });

    // 6c. 最大化状态变化（切换标题栏最大化 / 还原图标）
    // 6c. Maximized state changes (toggles the maximize/restore icon)
    platform::WindowFrame frame_state = frame;
    frame_state.on_maximized_changed([weak](bool is_maximized) {
        if (auto strong = weak.lock()) {
            (*strong)->set_titlebar_maximized(is_maximized);
        }
    });

    // 6d. 双击标题栏切换最大化 / Double-click toggles maximize
    platform::WindowFrame frame_dbl = frame;
    terminal->on_double_click([frame_dbl]() mutable { frame_dbl.toggle_maximized(); });

    // 6e. 复制日志 / Copy log
    const auto weak_log = weak;
    terminal->on_copy_log([weak_log] {
        if (weak_log.lock()) {
            std::printf("[CustomTerminal] 复制日志（功能预留） / Copy log (feature reserved)\n");
        }
    });

    // ── 7. 显示窗口（触发 CreateWindowExW → CBT Hook → apply_to_hwnd） ──
    // ── 7. Show the window (triggers CreateWindowExW → CBT Hook → apply_to_hwnd) ──
    terminal->window().show();

    // 若 CBT Hook 未能捕获（例如被其它窗口先激活），退化为事件循环中的补丁安装
    // If the hook did not capture the window, fall back to installing from the event loop
    if (hook_ok) {
        platform::WindowFrame frame_fallback = frame;
        slint::Timer::single_shot(std::chrono::milliseconds(0), [frame_fallback]() mutable {
            if (frame_fallback.hwnd() == nullptr) {
                frame_fallback.apply();
            }
        });
    }

    // ── 8. 保存组件实例到持有器 / Store the component into the holder ──
    handle->component = std::move(terminal);
    return true;
}

} // namespace windows::custom_terminal
