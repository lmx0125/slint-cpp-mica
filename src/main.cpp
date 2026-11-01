//! 程序入口：窗口创建、CBT Hook 编排与事件循环
//! Entry point: window creation, CBT Hook orchestration and the event loop.
#include <windows.h>
#include <dwmapi.h>

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

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif
#ifndef DWMWA_WINDOW_CORNER_PREFERENCE
#define DWMWA_WINDOW_CORNER_PREFERENCE 33
#endif
#ifndef DWMWCP_DONOTROUND
#define DWMWCP_DONOTROUND 1
#endif
#ifndef DWMWCP_ROUND
#define DWMWCP_ROUND 2
#endif
#ifndef DWMWCP_ROUNDSMALL
#define DWMWCP_ROUNDSMALL 3
#endif


typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,         // Aero 纯净晶透高斯毛玻璃
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,  // 超透可调亚克力
    ACCENT_ENABLE_HOSTBACKDROP = 5,
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;
    DWORD GradientColor;
    DWORD AnimationId;
} ACCENT_POLICY;

typedef struct _WINCOMPATTRDATA {
    DWORD Attribute; // 19 = WCA_ACCENT_POLICY
    PVOID Data;
    ULONG SizeOfData;
} WINCOMPATTRDATA;

typedef BOOL (WINAPI *pfnSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA*);

static void apply_accent_policy(HWND hwnd, ACCENT_STATE state, DWORD gradient_color = 0) {
    static auto pSetWindowCompositionAttribute = (pfnSetWindowCompositionAttribute)
        ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
    if (pSetWindowCompositionAttribute && hwnd) {
        ACCENT_POLICY policy{};
        policy.AccentState = state;
        policy.AccentFlags = 2;
        policy.GradientColor = gradient_color;
        
        WINCOMPATTRDATA data{};
        data.Attribute = 19; // WCA_ACCENT_POLICY
        data.Data = &policy;
        data.SizeOfData = sizeof(policy);
        pSetWindowCompositionAttribute(hwnd, &data);
    }
}

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

    // 6b. 绑定动效日志流演示交互 / Bind motion demo log callbacks
    auto demo_logs_model = std::make_shared<slint::VectorModel<slint::SharedString>>(
        std::vector<slint::SharedString>{
            "[00:00.001] [System] Slint C++ Fluent engine initialized",
            "[00:00.015] [Theme] Windows 11 Mica material applied to background",
            "[00:00.022] [Motion] Cubic-bezier timing curves registered",
            "[00:00.038] [DWM] Composition window frame subclass installed",
            "[00:00.050] [Ready] Demo components ready for interaction"
        });
    app->set_demo_logs(demo_logs_model);

    app->on_add_demo_log([demo_logs_model]() {
        static int event_count = 1;
        auto now = std::chrono::system_clock::now();
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()).count() % 100000;
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer), "[+%05lld ms] [Action #%d] Dynamic Fluent motion event triggered",
                      static_cast<long long>(ms), event_count++);
        demo_logs_model->push_back(slint::SharedString(buffer));
        std::printf("[Main] 追加动效演示日志: %s\n", buffer);
    });

    app->on_reset_demo_logs([demo_logs_model]() {
        demo_logs_model->set_vector({
            "[00:00.001] [System] Slint C++ Fluent engine initialized",
            "[00:00.015] [Theme] Windows 11 Mica material applied to background",
            "[00:00.022] [Motion] Cubic-bezier timing curves registered",
            "[00:00.038] [DWM] Composition window frame subclass installed",
            "[00:00.050] [Ready] Demo components ready for interaction"
        });
        std::printf("[Main] 重置动效演示日志样本\n");
    });

    app->on_clear_demo_logs([demo_logs_model]() {
        demo_logs_model->clear();
        std::printf("[Main] 已清空动效演示日志\n");
    });

    // 6c. 绑定 DWM 材质与圆角动态切换回调 / Bind dynamic DWM backdrop & corner switching
    auto app_weak = slint::ComponentWeakHandle<ui::AppWindow>(app);
    app->on_change_dwm_backdrop([app_weak, frame_holder](int index) {
        if (auto strong = app_weak.lock()) {
            (*strong)->set_dwm_backdrop_index(index);
            // index == 3 为 None 纯色表面，关闭窗口透明度；其余启用背景穿透
            (*strong)->set_is_mica_active(index != 3);
        }
        if (frame_holder && frame_holder->has_value()) {
            HWND hwnd = (*frame_holder)->hwnd();
            if (hwnd != nullptr) {
                // index: 0 -> Mica (2), 1 -> Acrylic (3, 硬件高斯虚化毛玻璃), 2 -> Tabbed (4), 3 -> None (1)
                uint32_t type = (index == 0) ? 2 : (index == 1) ? 3 : (index == 2) ? 4 : 1;
                ::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
                ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
                ::InvalidateRect(hwnd, nullptr, TRUE);
                std::printf("[Main] DWM 硬件材质已切换为: %u (index: %d, 1=Acrylic 硬件高斯虚化)\n", type, index);
            }
        }
    });

    app->on_change_dwm_corner([app_weak, frame_holder](int radius) {
        if (auto strong = app_weak.lock()) {
            (*strong)->set_dwm_corner_radius(radius);
        }
        if (frame_holder && frame_holder->has_value()) {
            HWND hwnd = (*frame_holder)->hwnd();
            if (hwnd != nullptr) {
                uint32_t pref = (radius >= 8) ? DWMWCP_ROUND : (radius > 0) ? DWMWCP_ROUNDSMALL : DWMWCP_DONOTROUND;
                ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
                ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                               SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
                ::InvalidateRect(hwnd, nullptr, TRUE);
                std::printf("[Main] DWM 窗口圆角已动态切换为: %u (radius: %d)\n", pref, radius);
            }
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
