//! DWM 原生按钮子类化模块
//! DWM native caption buttons subclassing module.
//!
//! 为 native-terminal-window 提供简化子类化：客户区扩展到整个窗口，
//! 由 DWM 系统原生绘制最小化 / 最大化 / 关闭按钮，Slint 只负责拖拽区渲染。
//!
//! Simplified subclassing for the native terminal: the client area extends over the
//! whole window so DWM draws the native caption buttons, while Slint only renders the
//! drag area. All window controls are handled natively through Win32 messages.
#pragma once

#include <windows.h>

#include <functional>
#include <memory>

namespace windows::native_terminal {

/// 标题栏高度（逻辑像素，与 titlebar.slint 中的 height: 36px 一致）
/// Titlebar height in logical pixels (matches height: 36px in titlebar.slint)
inline constexpr int kTitlebarHeight = 36;

/// DWM 原生按钮窗口框架 / DWM native caption frame
///
/// 1. WM_NCCALCSIZE — 客户区覆盖整个窗口，DWM 在客户区之上绘制原生按钮；
/// 2. WM_NCHITTEST — 标题栏返回 HTCAPTION，边缘返回拉伸码，按钮区域精确命中；
/// 3. 1px 阴影修复 — DwmExtendFrameIntoClientArea。
///
/// 1. WM_NCCALCSIZE — the client area covers the whole window; DWM paints the buttons
///    on top of it;
/// 2. WM_NCHITTEST — the titlebar returns HTCAPTION, the edges return resize codes and
///    the button area is hit precisely;
/// 3. 1px shadow fix — DwmExtendFrameIntoClientArea.
class NativeCaptionFrame
{
public:
    NativeCaptionFrame() = default;
    NativeCaptionFrame(const NativeCaptionFrame&) = default;
    NativeCaptionFrame& operator=(const NativeCaptionFrame&) = default;

    /// 开始拖拽窗口（Win32 原生拖拽，支持 Aero Snap）
    /// Begin a native Win32 drag (supports Aero Snap)
    void drag() const;
    /// 切换最大化 / 还原（经由 WM_SYSCOMMAND 走系统原生路径）
    /// Toggle maximize / restore through the native WM_SYSCOMMAND path
    void toggle_maximized() const;

    /// 在隐藏的 HWND 上安装子类化与 DWM 边框，必须在首次显示前调用
    /// Install the subclassing and DWM frame on a hidden HWND before the first show
    bool apply_to_hwnd(HWND hwnd);

    /// 当前框架关联的 HWND / Currently associated HWND
    HWND hwnd() const { return hwnd_; }

    /// 设置激活状态变更回调（由 WM_ACTIVATE 驱动，用于标题栏失焦变淡）
    /// Set the activation-state callback (driven by WM_ACTIVATE for the inactive fade)
    void on_active_changed(std::function<void(bool)> callback);

    /// 内部状态（公开以便窗口过程访问，属于实现细节）
    /// Internal state (public so the window proc can reach it; implementation detail)
    struct State;

private:
    HWND hwnd_ = nullptr;
    std::shared_ptr<State> state_;
};

} // namespace windows::native_terminal
