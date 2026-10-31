//! 视觉特效模块：配置 Slint 窗口背景透明度与 DWM Mica/Acrylic 联动
//! Visual effects: activates the Slint transparent background in coordination
//! with the DWM Mica/Acrylic material.
#pragma once

#include <functional>

namespace platform {

/// 为 Slint 窗口应用 Mica 视觉特效 / Apply the Mica visual effect
///
/// @param activate_mica     把组件背景切换为透明的回调（纯属性赋值，无需事件循环）
///                          Switches the component background to transparent
/// @param hook_will_handle  CBT Hook 是否已安装（已在 CreateWindowExW 瞬间注入 DWM 属性）
///                          Whether the CBT Hook injected the DWM attributes already
void apply_mica_effect(const std::function<void()>& activate_mica, bool hook_will_handle);

} // namespace platform
