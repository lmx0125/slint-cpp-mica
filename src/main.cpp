//! 程序入口：窗口创建、CBT Hook 编排与事件循环
//! Entry point: window creation, CBT Hook orchestration and the event loop.
#include <chrono>
#include <cstdio>
#include <memory>

#include "app-window.h"
#include "platform/borderless.h"
#include "platform/display.h"
#include "platform/effects.h"
#include "platform/hook.h"
#include "windows/app/app.h"
#include "windows/app/attributes.h"
#include "windows/custom_terminal/custom_terminal.h"
#include "windows/native_terminal/native_terminal.h"
#include "windows/windows.h"

namespace {

/// 主窗口标题（须与 .slint 中的 `title` 一致，用于 CBT Hook 匹配）
/// Main window title (must match the `title` property in the .slint file)
constexpr const wchar_t* kMainWindowTitle = L"Slint标准组件全家桶";

} // namespace

int main(int /*argc*/, char** /*argv*/)
{
    // ── 共享 Frame 持有器：CBT Hook 回调中同步安装子类化 + 阴影 ──
    // 主窗口通过 window().show() / 事件循环创建，窗口创建发生在内部。
    // CBT Hook 在 CreateWindowExW 瞬间捕获 HWND 并通过 apply_to_hwnd 同步安装，
    // 首帧绘制前一切就绪，彻底消除阴影与子类化的延迟。
    // ── Shared frame holder: install the subclassing and shadow synchronously from the
    //    CBT Hook. Slint creates the window inside the loop; the hook captures the HWND
    //    at CreateWindowExW time so everything is ready before the first frame ──
    auto frame_holder = std::make_shared<std::optional<platform::WindowFrame>>();

    // 1. 安装主窗口 CBT Hook（在 CreateWindowExW 瞬间注入 DWM 属性 + 子类化 + 阴影）
    // 1. Install the main window CBT Hook
    auto hook = platform::CbtHookGuard::install(
        std::wstring(kMainWindowTitle),
        [frame_holder](HWND hwnd) {
            // DWM 属性注入（Mica / 暗色模式 / 圆角等）
            // Inject the DWM attributes (Mica, dark mode, rounded corners, ...)
            auto attributes = windows::app::attributes::get_attributes();
            attributes.apply(hwnd);

            // 子类化 + 阴影（通过 HWND 同步安装，无需等待 winit 就绪）
            // Subclassing + shadow, installed synchronously via the HWND
            if (frame_holder->has_value()) {
                (*frame_holder)->apply_to_hwnd(hwnd);
            }
        });

    const bool hook_ok = hook.has_value();
    if (hook_ok) {
        std::printf("[Main] CBT Hook 已安装，等待窗口创建时捕获 UI 窗口 / CBT Hook installed, "
                    "waiting to capture the window\n");
        // 保留到 main 结束会阻止自卸载，这里让 Hook 在回调后自卸载
        // Let the hook unhook itself after the callback
        hook->detach();
    } else {
        std::printf("[Main] CBT Hook 安装失败，将通过 event loop 路径 fallback / CBT Hook "
                    "installation failed, falling back to the event loop path\n");
    }

    // 2. 创建主窗口实例 / Create the main window instance
    auto app = ui::AppWindow::create();

    // 3. 创建无边框框架（不调度 apply，由 CBT Hook 同步安装）
    // 3. Create the borderless frame (installed by the hook, not scheduled here)
    platform::WindowFrame frame = windows::app::create_frame(app);
    *frame_holder = frame;

    // 4. 配置主窗口（居中定位、Mica 特效、控件绑定）
    // 4. Configure the main window (centering, Mica, control bindings)
    windows::app::setup(app, frame, hook_ok);

    // 5. 绑定"打开自绘终端窗口"按钮（关闭时销毁，改为 CloseBehavior::Hide 可保留状态）
    // 5. Bind the "open custom terminal" button (Destroy on close; use Hide to keep state)
    auto custom_handle = std::make_shared<windows::custom_terminal::WindowHolder>();
    app->on_open_custom_terminal_window([custom_handle] {
        if (windows::custom_terminal::open(custom_handle, windows::CloseBehavior::Destroy)) {
            std::printf("[Main] 自绘终端窗口已打开 / Custom terminal window opened\n");
        } else {
            std::printf("[Main] 打开自绘终端窗口失败 / Failed to open the custom terminal\n");
        }
    });

    // 6. 绑定"打开原生终端窗口"按钮
    // 6. Bind the "open native terminal" button
    auto native_handle = std::make_shared<windows::native_terminal::WindowHolder>();
    app->on_open_native_terminal_window([native_handle] {
        if (windows::native_terminal::open(native_handle, windows::CloseBehavior::Destroy)) {
            std::printf("[Main] 原生终端窗口已打开 / Native terminal window opened\n");
        } else {
            std::printf("[Main] 打开原生终端窗口失败 / Failed to open the native terminal\n");
        }
    });

    // 7. 运行主循环 / Run the event loop
    app->window().show();

    // CBT Hook 未捕获时（例如没有 HCBT_ACTIVATE），在事件循环里补装
    // Patch from the event loop when the hook did not capture the window
    if (!hook_ok) {
        platform::WindowFrame frame_fallback = frame;
        slint::Timer::single_shot(std::chrono::milliseconds(0), [frame_fallback]() mutable {
            if (frame_fallback.hwnd() == nullptr) {
                frame_fallback.apply();
            }
        });
    }

    slint::run_event_loop();
    return 0;
}
