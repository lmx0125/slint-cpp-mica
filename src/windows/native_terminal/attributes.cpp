#include "windows/native_terminal/attributes.h"

#include <dwmapi.h>

namespace windows::native_terminal::attributes {

platform::WindowAttributes get_attributes()
{
    platform::WindowAttributes attributes;
    attributes.with_mica()
        .with_dark_mode(platform::is_system_dark_mode())
        .with_corner(platform::CornerPreference::Round)
        // Win11 会为带 WS_CAPTION 的窗口绘制不透明标题栏底色，覆盖在透明的 Slint
        // 标题栏上形成灰色横条；设为 DWMWA_COLOR_NONE 让 Mica 材质透出，
        // 原生按钮的绘制与交互不受影响。
        // Win11 paints an opaque caption background over the transparent Slint
        // titlebar, producing a gray strip. DWMWA_COLOR_NONE lets the Mica
        // backdrop show through while native caption buttons keep working.
        .with_caption_color(DWMWA_COLOR_NONE);
    // no-frame: false 已提供 WS_OVERLAPPEDWINDOW（含 CAPTION / SYSMENU / MINIMIZEBOX /
    // MAXIMIZEBOX），DWM 在窗口创建时即分配标题栏按钮，无需额外样式。
    // no-frame: false already provides WS_OVERLAPPEDWINDOW, so DWM allocates the caption
    // buttons at window creation; no extra styles are required.
    return attributes;
}

} // namespace windows::native_terminal::attributes
