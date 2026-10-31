//! 主窗口专用属性配置（DIY 空间）
//! Main window exclusive attributes configuration (DIY area).
//!
//! 在此自由定义主窗口的 DWM 材质、暗色模式、圆角、任务栏行为与逃生通道。
//! Configure the main window DWM material, dark mode, corners, taskbar presence and
//! escape hatches freely here.
#pragma once

#include "platform/attributes.h"

namespace windows::app::attributes {

/// 获取主窗口的个性化属性配置 / Main window attribute configuration
platform::WindowAttributes get_attributes();

} // namespace windows::app::attributes
