//! 显示器与窗口定位模块：根据鼠标所在显示器进行 DPI 换算并居中窗口，防止超屏。
//! Display and window positioning: DPI-aware centering on the monitor under the
//! cursor, with bounds clamping.
#pragma once

namespace slint {
class Window;
}

namespace platform {

/// 根据当前鼠标所在屏幕计算 DPI 并调整窗口居中定位，防止超屏
/// Center the window in the work area of the monitor under the cursor, clamped
/// to the work area size.
void center_on_active_monitor(slint::Window& window,
                              float init_w_logical,
                              float init_h_logical);

} // namespace platform
