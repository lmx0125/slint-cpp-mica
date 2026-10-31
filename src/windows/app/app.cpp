#include "windows/app/app.h"

#include <memory>

#include "app-window.h"
#include "platform/controls.h"
#include "platform/display.h"
#include "platform/effects.h"
#include "windows/app/attributes.h"
#include "windows/app/controls.h"

namespace windows::app {

platform::WindowFrame create_frame(const slint::ComponentHandle<ui::AppWindow>& app)
{
    const slint::ComponentWeakHandle<ui::AppWindow> weak(app);
    platform::TitlebarButtons buttons; // 三个按钮全部显示 / All three buttons visible
    auto adapter = std::make_shared<GlobalWindowControlsAdapter>(weak, buttons);
    return platform::WindowFrame(platform::make_window_accessor(weak), adapter);
}

void setup(const slint::ComponentHandle<ui::AppWindow>& app,
           const platform::WindowFrame& frame,
           bool hook_ok)
{
    // 1. 定位到当前活动屏幕居中 / Center on the active monitor
    platform::center_on_active_monitor(
        app->window(), app->get_init_width(), app->get_init_height());

    // 2. 应用 Mica 视觉特效 / Apply the Mica visual effect
    platform::apply_mica_effect([&app] { app->set_is_mica_active(true); }, hook_ok);

    // 3. 最大化 / 还原状态同步到 UI / Sync the maximize state into the UI
    platform::WindowFrame frame_for_state = frame;
    const slint::ComponentWeakHandle<ui::AppWindow> weak(app);
    frame_for_state.on_maximized_changed([weak](bool is_maximized) {
        if (auto strong = weak.lock()) {
            (*strong)->global<ui::WindowControls>().set_maximized(is_maximized);
        }
    });

    // 4. 绑定窗口控制回调 / Bind the window control callbacks
    platform::TitlebarButtons buttons;
    setup_global_window_controls(app, frame, buttons);
}

} // namespace windows::app
