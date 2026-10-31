//! app 模块：主窗口专属生命周期与业务逻辑
//! App module: main window lifecycle and business logic.
//!
//! 与 ui/app-window/ 一一对应，底层无边框与属性能力复用 platform 基础设施。
//! Mirrors ui/app-window/; reuses the platform infrastructure for borderless and DWM.
#pragma once

#include "platform/borderless.h"
#include "slint.h"

namespace ui {
class AppWindow;
}

namespace windows::app {

/// 创建主窗口的无边框框架（不调度 apply()，由 CBT Hook 在 HCBT_ACTIVATE 时同步安装）
/// Create the main window's borderless frame. apply() is not scheduled here: the CBT
/// Hook installs everything synchronously during HCBT_ACTIVATE.
platform::WindowFrame create_frame(const slint::ComponentHandle<ui::AppWindow>& app);

/// 配置主窗口（居中定位、Mica 特效、控件绑定）
/// Configure the main window (centering, Mica effect, control bindings)
void setup(const slint::ComponentHandle<ui::AppWindow>& app,
           const platform::WindowFrame& frame,
           bool hook_ok);

} // namespace windows::app
