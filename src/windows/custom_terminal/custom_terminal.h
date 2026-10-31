//! custom_terminal 子模块：与 ui/custom-terminal-window/ 一一对应
//! Custom terminal sub-module mirroring ui/custom-terminal-window/.
//!
//! 提供自绘终端窗口的创建、CBT Hook 防闪烁注入、回调绑定与 Mica 视觉联动。
//! Window creation, CBT Hook zero-flicker injection, callback binding and Mica effects.
#pragma once

#include <memory>
#include <optional>

#include "slint.h"
#include "windows/windows.h"

namespace ui {
class CustomTerminalWindow;
}

namespace windows::custom_terminal {

/// 终端窗口标题（必须与 .slint 中的 `title` 完全一致，用于 CBT Hook 匹配）
/// Terminal window title (must match the `title` property in the .slint file)
inline constexpr const wchar_t* kTerminalTitle = L"自绘终端日志 (Console Output)";

/// 窗口实例持有器：Destroy 行为下关闭时清空，Hide 行为下保留以复用
/// Instance holder: cleared on close in Destroy mode, kept for reuse in Hide mode
struct WindowHolder {
    std::optional<slint::ComponentHandle<ui::CustomTerminalWindow>> component;
};

using WindowHolderPtr = std::shared_ptr<WindowHolder>;

/// 创建并显示自绘终端窗口（含 CBT Hook 防闪烁 + 子类化安装）
/// Create and show the custom terminal window (CBT Hook anti-flicker + subclassing)
///
/// @param handle         外部持有的窗口实例 / External instance holder
/// @param close_behavior 关闭行为：隐藏 vs 销毁 / Hide vs destroy on close
/// @return 成功返回 true / true on success
bool open(const WindowHolderPtr& handle, CloseBehavior close_behavior);

} // namespace windows::custom_terminal
