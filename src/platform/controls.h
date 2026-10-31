//! 标题栏按钮可见性配置 / Titlebar button visibility configuration
#pragma once

namespace platform {

/// 标题栏按钮可见性配置 / Titlebar button visibility configuration
///
/// 同时影响两处 / Affects two things at once:
///  1. Win32 `WM_NCHITTEST`：隐藏的按钮不再参与命中测试（悬停不会触发 Snap Layouts）；
///  2. Slint UI：驱动 `show-minimize` / `show-maximize` / `show-close` 属性隐藏按钮。
///
///  1. Win32 hit-testing: hidden buttons no longer answer, so hovering blank space does
///     not trigger Snap Layouts;
///  2. Slint UI: drives the show-minimize / show-maximize / show-close properties.
struct TitlebarButtons {
    bool show_minimize = true;
    bool show_maximize = true;
    bool show_close = true;
};

} // namespace platform
