//! 自绘终端窗口专用属性配置（DIY 空间）
//! Custom terminal window attributes (DIY area).
//!
//! 在此自由调整 DWM 材质（Mica/Acrylic）、工具窗样式、置顶属性等。
//! Adjust the DWM material (Mica/Acrylic), tool-window style, always-on-top, etc.
#pragma once

#include "platform/attributes.h"

namespace windows::custom_terminal::attributes {

/// 获取自绘终端窗口的个性化属性配置 / Attribute configuration for the custom terminal
platform::WindowAttributes get_attributes();

} // namespace windows::custom_terminal::attributes
