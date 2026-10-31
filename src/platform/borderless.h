//! 无边框窗口框架：Win32 子类化、8 方向拉伸、Snap Layouts 命中测试与标题栏按钮交互
//! Borderless window frame: Win32 subclassing, 8-direction resize, Snap Layouts
//! hit-testing and titlebar button interaction.
#pragma once

#include <windows.h>

#include <functional>
#include <memory>
#include <optional>

namespace slint {
class Window;
}

namespace platform {

/// 标题栏度量尺寸配置（逻辑像素，会随后窗口 DPI 缩放）
/// Titlebar metrics in logical pixels (scaled by the window DPI)
struct TitlebarMetrics {
    float titlebar_height = 32.0f;
    float close_width = 46.0f;
    float maximize_width = 46.0f;
    float minimize_width = 46.0f;
    bool show_minimize = true;
    bool show_maximize = true;
    bool show_close = true;
};

class WindowFrame;

/// 标题栏适配器接口：连接 Win32 消息与特定 Slint 窗口的 UI 状态
/// Titlebar adapter: connects Win32 messages with a concrete Slint window's UI state
///
/// 每个窗口类型实现自己的适配器（对应 Rust 版中 `TitlebarAdapter<T>` 的具体实现）。
/// Each window type implements its own adapter (the Rust `TitlebarAdapter<T>` impls).
class TitlebarAdapter
{
public:
    virtual ~TitlebarAdapter() = default;

    /// 获取当前窗口的标题栏几何尺寸与按钮可见性
    /// Current titlebar geometry and button visibility
    virtual TitlebarMetrics metrics() const = 0;

    /// 窗口激活状态改变（聚焦 / 失焦）
    virtual void set_active(bool /*is_active*/) {}

    /// 按钮悬停状态更新 (min, max, close)
    virtual void set_hover(bool /*min*/, bool /*max*/, bool /*close*/) {}

    /// 按钮按下状态更新 (min, max, close)
    virtual void set_pressed(bool /*min*/, bool /*max*/, bool /*close*/) {}

    /// 点击最小化按钮 / Minimize clicked
    virtual void on_minimize_clicked(WindowFrame& frame);
    /// 点击最大化 / 还原按钮 / Maximize clicked
    virtual void on_maximize_clicked(WindowFrame& frame);
    /// 点击关闭按钮 / Close clicked
    virtual void on_close_clicked(WindowFrame& frame);
};

/// 默认空适配器（仅提供基础边缘拉伸与原生阴影，不拦截非客户区控制按钮）
/// Default empty adapter: only edge resizing and native shadow, no caption buttons
class SimpleBorderlessAdapter : public TitlebarAdapter
{
public:
    TitlebarMetrics metrics() const override
    {
        TitlebarMetrics m;
        m.show_minimize = false;
        m.show_maximize = false;
        m.show_close = false;
        return m;
    }
};

/// 无边框窗口框架 / Borderless window frame
///
/// 所有窗口类型共用同一份 Win32 子类化逻辑，通过 `TitlebarAdapter` 把消息转发给
/// 具体的 Slint 组件，避免为每个窗口重复实现窗口过程。
///
/// All window types share one Win32 subclassing implementation; the `TitlebarAdapter`
/// forwards messages to the concrete Slint component.
class WindowFrame
{
public:
    /// 窗口访问器：返回 Slint 窗口对象，组件已销毁时返回 nullptr
    /// Window accessor: returns the Slint window, or nullptr once the component is gone
    using WindowAccessor = std::function<slint::Window*()>;

    WindowFrame() = default;
    WindowFrame(WindowAccessor accessor, std::shared_ptr<TitlebarAdapter> adapter);

    WindowFrame(const WindowFrame&) = default;
    WindowFrame& operator=(const WindowFrame&) = default;

    bool is_valid() const { return state_ != nullptr; }

    /// 设置窗口最大化状态 / Set maximized state
    void maximize(bool is_maximized);
    /// 切换最大化 / 还原 / Toggle maximized
    void toggle_maximized();
    /// 最小化窗口 / Minimize
    void minimize();
    /// 关闭（隐藏）当前窗口 / Close (hide) the window
    void close();
    /// 开始拖拽窗口（Win32 原生拖拽，支持 Aero Snap）
    /// Begin a native Win32 drag (supports Aero Snap)
    void drag();
    /// 启用无边框窗口阴影（DWM 渲染）/ Enable the undecorated window shadow
    void set_undecorated_shadow(bool enabled);

    /// 应用无边框窗口样式与子类化（需要 HWND 已创建）
    /// Apply the borderless styles and subclassing (requires an existing HWND)
    void apply();

    /// 仅使用 HWND 安装子类化（CBT Hook 等场景）
    /// Install the subclassing using only the HWND (CBT Hook scenarios)
    void apply_to_hwnd(HWND hwnd);

    /// 设置最大化状态变更回调 / Set the maximized-state-changed callback
    void on_maximized_changed(std::function<void(bool)> callback);

    /// 当前框架关联的 HWND（可能为空）/ Currently associated HWND (may be null)
    HWND hwnd() const;

    /// 内部状态（公开以便窗口过程访问，属于实现细节）
    /// Internal state (public so the window proc can reach it; implementation detail)
    struct State;

private:

    void with_window(const std::function<void(slint::Window&)>& f);

    std::shared_ptr<State> state_;
};

/// 通用工具：由 Slint 组件弱引用构造 WindowAccessor
/// Helper: build a WindowAccessor from a Slint weak component handle
template <typename ComponentWeak>
WindowFrame::WindowAccessor make_window_accessor(ComponentWeak weak)
{
    return [weak]() -> slint::Window* {
        auto strong = weak.lock();
        if (!strong) {
            return nullptr;
        }
        return &(*strong)->window();
    };
}

} // namespace platform
