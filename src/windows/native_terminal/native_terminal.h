//! native_terminal 子模块：与 ui/native-terminal-window/ 一一对应
//! Native terminal sub-module mirroring ui/native-terminal-window/.
//!
//! 窗口以 no-frame: false 创建，标题栏按钮由 DWM 系统原生绘制；Slint 只负责拖拽区。
//! 与自绘终端的关键差别：本窗口不在 HCBT_ACTIVATE 里刷新边框，而是在隐藏状态下
//! 完成 DWM 属性与子类化配置后再显示，避免激活期间的边框闪烁。
//!
//! The window is created with no-frame: false and DWM renders the caption buttons;
//! Slint only provides the drag area. Unlike the custom terminal, the DWM frame is
//! configured while the window is still hidden (via the CBT hook, before the first
//! paint) so activation does not flash a frame.
#pragma once

#include <memory>
#include <optional>

#include "slint.h"
#include "windows/windows.h"

namespace ui {
class NativeTerminalWindow;
}

namespace windows::native_terminal {

/// 窗口实例持有器 / Instance holder
struct WindowHolder {
    std::optional<slint::ComponentHandle<ui::NativeTerminalWindow>> component;
};

using WindowHolderPtr = std::shared_ptr<WindowHolder>;

/// 创建并显示原生终端窗口（DWM 原生标题栏按钮）
/// Create and show the native terminal window (DWM native caption buttons)
bool open(const WindowHolderPtr& handle, CloseBehavior close_behavior);

} // namespace windows::native_terminal
