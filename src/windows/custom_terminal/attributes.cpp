#include "windows/custom_terminal/attributes.h"

namespace windows::custom_terminal::attributes {

platform::WindowAttributes get_attributes()
{
    platform::WindowAttributes attributes;
    attributes.with_mica()                                        // 也可换为 with_acrylic()
        .with_dark_mode(platform::is_system_dark_mode())
        .with_corner(platform::CornerPreference::Round);
    // 示例：如需使终端变为轻量工具悬浮窗，取消下面注释
    // Example: uncomment to make the terminal a lightweight tool window
    //   .with_tool_window(true)
    // 示例：如需终端置顶显示 / Example: always on top
    //   .with_always_on_top(true)
    return attributes;
}

} // namespace windows::custom_terminal::attributes
