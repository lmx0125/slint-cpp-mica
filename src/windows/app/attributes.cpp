#include "windows/app/attributes.h"

namespace windows::app::attributes {

platform::WindowAttributes get_attributes()
{
    platform::WindowAttributes attributes;
    attributes
        .with_mica()                            // Win11 Mica 材质 / Win11 Mica material
        .with_dark_mode(platform::is_system_dark_mode()) // 跟随系统深浅色 / Follow system theme
        .with_corner(platform::CornerPreference::Round)  // 标准大圆角 / Standard rounded corners
        .with_app_window(true);                          // 任务栏显示图标 / Show in taskbar
    return attributes;
}

} // namespace windows::app::attributes
