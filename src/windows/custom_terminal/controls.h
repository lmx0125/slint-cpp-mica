//! 自绘终端窗口专用标题栏适配器
//! Custom titlebar adapter for the custom terminal window.
//!
//! 提供与主窗口一致的非客户区按钮命中测试，从而获得系统原生 tooltip 与
//! Win11 Snap Layouts；与主窗口不同的是这里直接驱动 CustomTerminalWindow 的
//! titlebar-* 属性，而不依赖 global WindowControls 单例。
//!
//! Provides the same non-client hit testing as the main window (native tooltips and
//! Win11 Snap Layouts), but drives the CustomTerminalWindow titlebar-* properties
//! directly instead of the global WindowControls singleton.
#pragma once

#include <memory>

#include "platform/borderless.h"
#include "platform/controls.h"
#include "slint.h"

namespace ui {
class CustomTerminalWindow;
}

namespace windows::custom_terminal {

/// 终端窗口专用标题栏适配器 / Titlebar adapter for the terminal window
class TerminalTitlebarAdapter : public platform::TitlebarAdapter
{
public:
    TerminalTitlebarAdapter(slint::ComponentWeakHandle<ui::CustomTerminalWindow> component,
                            platform::TitlebarButtons buttons);

    platform::TitlebarMetrics metrics() const override;
    void set_active(bool is_active) override;
    void set_hover(bool min, bool max, bool close) override;
    void set_pressed(bool min, bool max, bool close) override;
    void on_close_clicked(platform::WindowFrame& frame) override;

private:
    slint::ComponentWeakHandle<ui::CustomTerminalWindow> component_;
    platform::TitlebarButtons buttons_;
};

} // namespace windows::custom_terminal
