//! 系统环境与版本检测模块
//! System environment and OS version detection module.
#pragma once

namespace sys_info {

/// 检测当前系统是否为 Windows 11 (Build >= 22000)
/// Detect whether the current OS is Windows 11 (Build >= 22000)
bool is_win11();

} // namespace sys_info
