//! 业务与示例窗口模块集合 / Application and sample window modules
//!
//! 包含主窗口、自绘终端窗口与 DWM 原生终端窗口的具体业务实现。
//! Contains the main window, the custom terminal window and the DWM native
//! caption-button terminal window.
#pragma once

namespace windows {

/// 窗口关闭行为 / Window close behavior
///
/// 控制二级窗口（终端窗口）被关闭时的行为：
/// - `Hide`:    隐藏窗口，保留组件实例，可稍后重新显示（适合频繁切换显示/隐藏）；
/// - `Destroy`: 销毁窗口，释放组件及所有资源（适合用完即弃，默认行为）。
///
/// Controls what happens when a secondary window (terminal) is closed:
/// - `Hide`:    hide the window and keep the component alive for later re-display;
/// - `Destroy`: hide and drop the component, releasing all resources (default).
enum class CloseBehavior {
    Hide,
    Destroy,
};

} // namespace windows
