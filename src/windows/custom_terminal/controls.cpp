#include "windows/custom_terminal/controls.h"

#include <utility>

#include "app-window.h"

namespace windows::custom_terminal {

TerminalTitlebarAdapter::TerminalTitlebarAdapter(
    slint::ComponentWeakHandle<ui::CustomTerminalWindow> component,
    platform::TitlebarButtons buttons)
    : component_(std::move(component)), buttons_(buttons)
{
}

namespace {

template <typename F>
void with_component(const slint::ComponentWeakHandle<ui::CustomTerminalWindow>& weak, F&& callback)
{
    if (auto strong = weak.lock()) {
        callback(*strong);
    }
}

} // namespace

platform::TitlebarMetrics TerminalTitlebarAdapter::metrics() const
{
    // 按钮尺寸与 ui/custom-terminal-window/titlebar.slint 中 TerminalTitlebar 的布局一致：
    // 高度 36px，按钮宽度 46px
    // Metrics match the TerminalTitlebar layout (height 36px, button width 46px)
    platform::TitlebarMetrics metrics;
    metrics.titlebar_height = 36.0f;
    metrics.close_width = 46.0f;
    metrics.maximize_width = 46.0f;
    metrics.minimize_width = 46.0f;
    metrics.show_minimize = buttons_.show_minimize;
    metrics.show_maximize = buttons_.show_maximize;
    metrics.show_close = buttons_.show_close;
    return metrics;
}

void TerminalTitlebarAdapter::set_active(bool is_active)
{
    with_component(component_, [is_active](slint::ComponentHandle<ui::CustomTerminalWindow> terminal) {
        if (terminal->get_titlebar_is_active() != is_active) {
            terminal->set_titlebar_is_active(is_active);
        }
    });
}

void TerminalTitlebarAdapter::set_hover(bool min, bool max, bool close)
{
    with_component(component_,
                   [min, max, close](slint::ComponentHandle<ui::CustomTerminalWindow> terminal) {
                       if (terminal->get_titlebar_min_hover() != min) {
                           terminal->set_titlebar_min_hover(min);
                       }
                       if (terminal->get_titlebar_max_hover() != max) {
                           terminal->set_titlebar_max_hover(max);
                       }
                       if (terminal->get_titlebar_close_hover() != close) {
                           terminal->set_titlebar_close_hover(close);
                       }
                   });
}

void TerminalTitlebarAdapter::set_pressed(bool min, bool max, bool close)
{
    with_component(component_,
                   [min, max, close](slint::ComponentHandle<ui::CustomTerminalWindow> terminal) {
                       if (terminal->get_titlebar_min_pressed() != min) {
                           terminal->set_titlebar_min_pressed(min);
                       }
                       if (terminal->get_titlebar_max_pressed() != max) {
                           terminal->set_titlebar_max_pressed(max);
                       }
                       if (terminal->get_titlebar_close_pressed() != close) {
                           terminal->set_titlebar_close_pressed(close);
                       }
                   });
}

void TerminalTitlebarAdapter::on_close_clicked(platform::WindowFrame& /*frame*/)
{
    // 委托给 Slint 的 close-requested 回调（由 open() 绑定关闭行为）
    // Delegate to the Slint close-requested callback bound in open()
    with_component(component_, [](slint::ComponentHandle<ui::CustomTerminalWindow> terminal) {
        terminal->invoke_close_requested();
    });
}

} // namespace windows::custom_terminal
