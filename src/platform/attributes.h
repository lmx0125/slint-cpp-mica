//! 窗口属性与样式配置模块：声明式配置 DWM 视觉属性（Mica / Acrylic / 暗色模式 / 圆角）
//! 与 Win32 窗口样式（工具窗口、置顶、穿透、扩展样式）及逃生通道。
//!
//! Window attributes and styles: declaratively configure DWM visual attributes
//! (Mica / Acrylic / dark mode / rounded corners) and Win32 window styles
//! (tool window, always on top, click-through, extended styles) with escape hatches.
#pragma once

#include <windows.h>

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

namespace platform {

/// 检测当前系统是否处于深色模式（读取注册表 AppsUseLightTheme）
/// Detect whether the system is in dark mode (reads AppsUseLightTheme)
bool is_system_dark_mode();

/// DWM 背景材质类型 / DWM backdrop material type
enum class DwmBackdrop {
    Mica,    ///< Mica 材质（Win11 主窗口推荐） / Mica (recommended for main windows)
    Acrylic, ///< Acrylic 亚克力材质 / Acrylic translucent blur
    Tabbed,  ///< Tabbed 标签页材质 / Tabbed
    None,    ///< 无 DWM 材质 / No backdrop
};

/// 圆角偏好 / Corner preference
enum class CornerPreference {
    Round,      ///< 标准大圆角 / Standard rounded
    RoundSmall, ///< 小圆角 / Small rounded
    DoNotRound, ///< 不圆角 / Not rounded
};

/// 声明式窗口属性配置器 / Declarative window attribute builder
///
/// 涵盖 / Covers:
///  1. DWM 视觉材质、暗色模式、圆角偏好、边框/标题/文字颜色；
///  2. Win32 窗口样式（工具窗口、置顶、穿透、不抢焦点、自定义样式位）；
///  3. 三级逃生通道（自定义样式位 / 自定义 DWM 属性 / 裸 HWND 回调）。
class WindowAttributes
{
public:
    WindowAttributes() = default;

    // ── 1. DWM 视觉属性 / DWM visual attributes ──────────────────────────
    WindowAttributes& with_mica();     ///< 启用 Mica 材质
    WindowAttributes& with_acrylic();  ///< 启用 Acrylic 材质
    WindowAttributes& with_tabbed();   ///< 启用 Tabbed 材质
    WindowAttributes& with_backdrop(DwmBackdrop backdrop);
    WindowAttributes& with_dark_mode(bool enabled);
    WindowAttributes& with_corner(CornerPreference pref);
    WindowAttributes& with_border_color(uint32_t color_bgr);  ///< 0x00BBGGRR
    WindowAttributes& with_caption_color(uint32_t color_bgr); ///< 0x00BBGGRR
    WindowAttributes& with_text_color(uint32_t color_bgr);    ///< 0x00BBGGRR

    // ── 2. Win32 窗口样式 / Win32 window styles ─────────────────────────
    WindowAttributes& with_tool_window(bool enabled);      ///< WS_EX_TOOLWINDOW
    WindowAttributes& with_app_window(bool enabled);       ///< WS_EX_APPWINDOW
    WindowAttributes& with_always_on_top(bool enabled);    ///< WS_EX_TOPMOST
    WindowAttributes& with_click_through(bool enabled);    ///< WS_EX_TRANSPARENT
    WindowAttributes& with_no_activate(bool enabled);      ///< WS_EX_NOACTIVATE
    WindowAttributes& with_raw_style(DWORD add, DWORD remove);
    WindowAttributes& with_raw_ex_style(DWORD add, DWORD remove);

    // ── 3. 逃生通道 / Escape hatches ───────────────────────────────────
    /// 逃生通道 1：向 HWND 直接注入任意 DWM 属性（DwmSetWindowAttribute）
    WindowAttributes& with_raw_dwm_attribute(uint32_t attr, const void* data, size_t size);
    /// 便捷重载：以值的形式注入（与 Rust 版 `with_raw_dwm_attribute<T>` 等价）
    template <typename T>
    WindowAttributes& with_raw_attribute(uint32_t attr, T value)
    {
        return with_raw_dwm_attribute(attr, &value, sizeof(T));
    }
    /// 逃生通道 2：在 apply(hwnd) 时执行任意 Win32 / DWM 调用
    WindowAttributes& with_custom_action(std::function<void(HWND)> action);

    /// 将配置好的属性与样式应用到目标 HWND
    /// Apply the configured attributes and styles to the target HWND
    void apply(HWND hwnd) const;

private:
    std::optional<DwmBackdrop> backdrop_;
    std::optional<bool> dark_mode_;
    std::optional<CornerPreference> corner_;
    std::optional<uint32_t> border_color_;
    std::optional<uint32_t> caption_color_;
    std::optional<uint32_t> text_color_;

    DWORD style_add_ = 0;
    DWORD style_remove_ = 0;
    DWORD ex_style_add_ = 0;
    DWORD ex_style_remove_ = 0;

    std::vector<std::pair<uint32_t, std::vector<uint8_t>>> raw_dwm_attrs_;
    std::vector<std::function<void(HWND)>> custom_actions_;
};

/// 向后兼容别名（与 Rust 版 `DwmPreset` 对应）
using DwmPreset = WindowAttributes;

} // namespace platform
