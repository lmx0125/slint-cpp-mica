//! 原生终端窗口专用属性配置（DIY 空间）
//! Native terminal window attributes (DIY area).
//!
//! 窗口使用 no-frame: false（WS_OVERLAPPEDWINDOW），标题栏按钮由 DWM 原生绘制，
//! 通过 WM_NCCALCSIZE 子类化隐藏标准标题栏、仅保留原生按钮。
//!
//! The window keeps no-frame: false (WS_OVERLAPPEDWINDOW) so DWM renders the caption
//! buttons natively; WM_NCCALCSIZE subclassing hides the standard titlebar while the
//! native buttons stay.
#pragma once

#include "platform/attributes.h"

namespace windows::native_terminal::attributes {

/// 获取原生终端窗口的个性化属性配置 / Attribute configuration for the native terminal
platform::WindowAttributes get_attributes();

} // namespace windows::native_terminal::attributes
