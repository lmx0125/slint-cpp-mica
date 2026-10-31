#include "platform/display.h"

#include <windows.h>

#include <shellscalingapi.h>

#include <algorithm>

#include "slint.h"

namespace platform {

void center_on_active_monitor(slint::Window& window,
                              float init_w_logical,
                              float init_h_logical)
{
    // 1. 获取当前鼠标物理位置 / Current physical mouse position
    POINT cursor_pos{};
    ::GetCursorPos(&cursor_pos);

    // 2. 获取目标显示器句柄 / Target monitor
    const HMONITOR monitor = ::MonitorFromPoint(cursor_pos, MONITOR_DEFAULTTONEAREST);

    // 3. 获取工作区 RECT / Work area
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfoW(monitor, &monitor_info)) {
        return;
    }
    const RECT work_area = monitor_info.rcWork;

    // 4. 获取显示器 DPI / Monitor DPI
    UINT dpi_x = 96;
    UINT dpi_y = 96;
    if (FAILED(::GetDpiForMonitor(monitor, MDT_EFFECTIVE_DPI, &dpi_x, &dpi_y))) {
        dpi_x = ::GetDpiForSystem();
    }
    const float scale = static_cast<float>(dpi_x) / 96.0f;

    const float work_left_phys = static_cast<float>(work_area.left);
    const float work_top_phys = static_cast<float>(work_area.top);
    const float work_w_phys = static_cast<float>(work_area.right - work_area.left);
    const float work_h_phys = static_cast<float>(work_area.bottom - work_area.top);

    // 5. 计算逻辑工作区 / Logical work area
    const float work_w_logical = work_w_phys / scale;
    const float work_h_logical = work_h_phys / scale;

    // 6. 防超屏截断 / Clamp to the work area
    const float target_w_logical = std::min(init_w_logical, work_w_logical);
    const float target_h_logical = std::min(init_h_logical, work_h_logical);
    const bool is_resized = target_w_logical < init_w_logical || target_h_logical < init_h_logical;

    // 7. 计算居中物理坐标 / Centered physical coordinates
    const float target_w_phys = target_w_logical * scale;
    const float target_h_phys = target_h_logical * scale;

    const int target_x_phys =
        static_cast<int>(work_left_phys + (work_w_phys - target_w_phys) / 2.0f);
    const int target_y_phys =
        static_cast<int>(work_top_phys + (work_h_phys - target_h_phys) / 2.0f);

    // 应用位置与尺寸 / Apply position and size
    window.set_position(slint::PhysicalPosition{slint::Point<int32_t>{target_x_phys, target_y_phys}});
    if (is_resized) {
        window.set_size(slint::LogicalSize{slint::Size<float>{target_w_logical, target_h_logical}});
    }
}

} // namespace platform
