//! 主窗口专用控件绑定配置（DIY 空间）
//! Main window control bindings (DIY area).
//!
//! 针对使用 Slint `global WindowControls` 单例的窗口提供通用适配器。
//! Universal adapter for windows that use the Slint `global WindowControls` singleton.
#pragma once

#include <memory>

#include "platform/borderless.h"
#include "platform/controls.h"
#include "slint.h"

namespace ui {
class AppWindow;
}

namespace windows::app {

/// 使用 Slint `global WindowControls` 单例的窗口适配器（主窗口）
/// Adapter for windows using the global WindowControls singleton (main window)
class GlobalWindowControlsAdapter : public platform::TitlebarAdapter
{
public:
    GlobalWindowControlsAdapter(slint::ComponentWeakHandle<ui::AppWindow> component,
                                platform::TitlebarButtons buttons);

    platform::TitlebarMetrics metrics() const override;
    void set_active(bool is_active) override;
    void set_hover(bool min, bool max, bool close) override;
    void set_pressed(bool min, bool max, bool close) override;
    void on_minimize_clicked(platform::WindowFrame& frame) override;
    void on_maximize_clicked(platform::WindowFrame& frame) override;
    void on_close_clicked(platform::WindowFrame& frame) override;

private:
    slint::ComponentWeakHandle<ui::AppWindow> component_;
    platform::TitlebarButtons buttons_;
};

/// 快速绑定主窗口 global WindowControls 的回调与同步
/// Bind the callbacks and synchronize state with the global WindowControls
void setup_global_window_controls(const slint::ComponentHandle<ui::AppWindow>& app,
                                  const platform::WindowFrame& frame,
                                  platform::TitlebarButtons buttons);

} // namespace windows::app
