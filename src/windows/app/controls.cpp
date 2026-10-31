#include "windows/app/controls.h"

#include <utility>

#include "app-window.h"

namespace windows::app {

GlobalWindowControlsAdapter::GlobalWindowControlsAdapter(
    slint::ComponentWeakHandle<ui::AppWindow> component,
    platform::TitlebarButtons buttons)
    : component_(std::move(component)), buttons_(buttons)
{
}

namespace {

/// 锁住弱引用并执行 / Lock the weak handle and run the callback
template <typename F>
void with_component(const slint::ComponentWeakHandle<ui::AppWindow>& weak, F&& callback)
{
    if (auto strong = weak.lock()) {
        callback(strong);
    }
}

} // namespace

platform::TitlebarMetrics GlobalWindowControlsAdapter::metrics() const
{
    platform::TitlebarMetrics metrics;
    with_component(component_, [&metrics, this](const std::optional<slint::ComponentHandle<ui::AppWindow>>& app) {
        const ui::WindowControls& controls = (*app)->global<ui::WindowControls>();
        metrics.titlebar_height = controls.get_titlebar_height();
        metrics.close_width = controls.get_close_width();
        metrics.maximize_width = controls.get_maximize_width();
        metrics.minimize_width = controls.get_minimize_width();
        metrics.show_minimize = buttons_.show_minimize && controls.get_show_minimize();
        metrics.show_maximize = buttons_.show_maximize && controls.get_show_maximize();
        metrics.show_close = buttons_.show_close && controls.get_show_close();
    });
    return metrics;
}

void GlobalWindowControlsAdapter::set_active(bool is_active)
{
    with_component(component_, [is_active](const auto& app) {
        (*app)->global<ui::WindowControls>().set_is_active(is_active);
    });
}

void GlobalWindowControlsAdapter::set_hover(bool min, bool max, bool close)
{
    with_component(component_, [min, max, close](const auto& app) {
        const ui::WindowControls& controls = (*app)->global<ui::WindowControls>();
        if (controls.get_min_hover() != min) {
            controls.set_min_hover(min);
        }
        if (controls.get_max_hover() != max) {
            controls.set_max_hover(max);
        }
        if (controls.get_close_hover() != close) {
            controls.set_close_hover(close);
        }
    });
}

void GlobalWindowControlsAdapter::set_pressed(bool min, bool max, bool close)
{
    with_component(component_, [min, max, close](const auto& app) {
        const ui::WindowControls& controls = (*app)->global<ui::WindowControls>();
        if (controls.get_min_pressed() != min) {
            controls.set_min_pressed(min);
        }
        if (controls.get_max_pressed() != max) {
            controls.set_max_pressed(max);
        }
        if (controls.get_close_pressed() != close) {
            controls.set_close_pressed(close);
        }
    });
}

void GlobalWindowControlsAdapter::on_minimize_clicked(platform::WindowFrame& /*frame*/)
{
    with_component(component_, [](const auto& app) {
        (*app)->global<ui::WindowControls>().invoke_minimize();
    });
}

void GlobalWindowControlsAdapter::on_maximize_clicked(platform::WindowFrame& /*frame*/)
{
    with_component(component_, [](const auto& app) {
        (*app)->global<ui::WindowControls>().invoke_maximize();
    });
}

void GlobalWindowControlsAdapter::on_close_clicked(platform::WindowFrame& /*frame*/)
{
    with_component(component_, [](const auto& app) {
        (*app)->global<ui::WindowControls>().invoke_close();
    });
}

void setup_global_window_controls(const slint::ComponentHandle<ui::AppWindow>& app,
                                  const platform::WindowFrame& frame,
                                  platform::TitlebarButtons buttons)
{
    const ui::WindowControls& controls = app->global<ui::WindowControls>();

    // 1. 初始化按钮可见性与状态 / Initialize button visibility and state
    controls.set_show_minimize(buttons.show_minimize);
    controls.set_show_maximize(buttons.show_maximize);
    controls.set_show_close(buttons.show_close);
    controls.set_maximized(false);

    // 2. 最大化 / 还原状态变化由 app.cpp 注册 / The maximize listener is set up in app.cpp

    // 3. 绑定 UI 回调到 WindowFrame / Bind UI callbacks to the WindowFrame
    platform::WindowFrame f_max = frame;
    controls.on_maximize([f_max]() mutable { f_max.toggle_maximized(); });

    platform::WindowFrame f_dbl = frame;
    controls.on_double_click([f_dbl]() mutable { f_dbl.toggle_maximized(); });

    platform::WindowFrame f_close = frame;
    controls.on_close([f_close]() mutable { f_close.close(); });

    platform::WindowFrame f_drag = frame;
    controls.on_drag([f_drag]() mutable { f_drag.drag(); });

    platform::WindowFrame f_min = frame;
    controls.on_minimize([f_min]() mutable { f_min.minimize(); });
}

} // namespace windows::app
