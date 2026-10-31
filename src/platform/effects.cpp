#include "platform/effects.h"

#include <cstdio>

#include "sys_info.h"

namespace platform {

void apply_mica_effect(const std::function<void()>& activate_mica, bool hook_will_handle)
{
    if (hook_will_handle) {
        // 检查系统是否支持 Mica（Windows 11 build 22000+）
        // Check whether the OS supports Mica (Windows 11 build 22000+)
        if (!sys_info::is_win11()) {
            std::printf("[Effects] 系统不支持 Mica (Win10 或更低版本)，已降级为系统自适应纯色背景 / "
                        "OS does not support Mica, falling back to a solid background\n");
            return;
        }

        // DWM Mica 属性已由 CBT Hook 注入，这里只需让 Slint 背景透明以透出材质。
        // set_is_mica_active 是纯属性赋值，无需事件循环，同步调用即可保证首帧即透明。
        // The DWM attributes were injected by the CBT Hook; we only need a transparent
        // Slint background. Setting the property is a plain assignment, so the very first
        // frame is already transparent.
        activate_mica();
        std::printf("[Effects] Mica UI 透明标志已激活（DWM 材质由 CBT Hook 注入） / "
                    "Mica transparency flag activated\n");
        return;
    }

    std::printf("[Effects] CBT Hook 注入未生效，已直接降级为系统自适应纯色背景 / "
                "CBT Hook injection inactive, falling back to a solid background\n");
}

} // namespace platform
