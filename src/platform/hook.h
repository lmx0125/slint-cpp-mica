//! 轻量级 CBT Hook 模块：在窗口创建瞬间提供 HWND，由调用方决定如何处理
//! Lightweight CBT Hook module: hands out the HWND at window creation time and
//! lets the caller decide what to do with it.
//!
//! 设计原则 / Design principles
//!  - 只负责 Hook + 识别窗口 + 交付 HWND，不绑定具体属性逻辑；
//!  - 提供 100% 开放的逃生通道：回调直接收到原始 Win32 HWND；
//!  - 不干预 borderless 逻辑，自绘标题栏的 hover/click 完全保留；
//!  - Hook 在回调完成后立即自卸载，不长期驻留。
//!
//!  - Focused on hook + target matching + HWND delivery, no attribute policy;
//!  - 100% open escape hatch: the callback receives the raw Win32 HWND;
//!  - Zero interference with the borderless logic;
//!  - Unhooks itself immediately after the callback returns.
#pragma once

#include <windows.h>

#include <functional>
#include <optional>
#include <string>

namespace platform {

/// CBT Hook 的 RAII 守卫 / RAII guard for the CBT hook
///
/// 安装后在 HCBT_ACTIVATE 中自动回调并卸载；若析构时 Hook 仍在（期间没有窗口创建），
/// 也会自动卸载。调用 detach() 可放弃所有权、让 Hook 自卸载（对应 Rust 的
/// `std::mem::forget(guard)`）。
///
/// Invokes the callback and unhooks in HCBT_ACTIVATE. If the guard is destroyed while the
/// hook is still installed (no window was created), it unhooks as well. detach() releases
/// ownership and lets the hook unhook itself (the Rust `mem::forget(guard)` equivalent).
class CbtHookGuard
{
public:
    CbtHookGuard() = default;
    CbtHookGuard(const CbtHookGuard&) = delete;
    CbtHookGuard& operator=(const CbtHookGuard&) = delete;
    CbtHookGuard(CbtHookGuard&& other) noexcept;
    CbtHookGuard& operator=(CbtHookGuard&& other) noexcept;
    ~CbtHookGuard();

    /// 在当前线程安装 WH_CBT Hook
    /// Install a WH_CBT hook on the current thread.
    ///
    /// @param target_title   可选窗口标题，提供时只对标题匹配的顶层窗口触发
    /// @param on_hwnd_ready  捕获到目标窗口 HWND 后执行的回调
    static std::optional<CbtHookGuard> install(std::optional<std::wstring> target_title,
                                               std::function<void(HWND)> on_hwnd_ready);

    /// 手动提前卸载 / Unhook early
    void uninstall();

    /// 放弃所有权（Hook 将在 HCBT_ACTIVATE 后自卸载）
    /// Release ownership (the hook unhooks itself after HCBT_ACTIVATE)
    void detach();

private:
    explicit CbtHookGuard(HHOOK hook) : hook_(hook) {}

    void do_uninstall();

    HHOOK hook_ = nullptr;
};

} // namespace platform
