#include "sys_info.h"

#include <windows.h>

namespace sys_info {

bool is_win11()
{
    // 缓存结果，避免重复调用 ntdll
    // Cache the result to avoid repeated ntdll calls
    static const bool cached = [] {
        using RtlGetVersionFn = LONG(WINAPI*)(PRTL_OSVERSIONINFOW);

        HMODULE ntdll = ::GetModuleHandleW(L"ntdll.dll");
        if (ntdll == nullptr) {
            return false;
        }
        auto rtl_get_version =
            reinterpret_cast<RtlGetVersionFn>(::GetProcAddress(ntdll, "RtlGetVersion"));
        if (rtl_get_version == nullptr) {
            return false;
        }

        RTL_OSVERSIONINFOW info{};
        info.dwOSVersionInfoSize = sizeof(info);
        if (rtl_get_version(&info) != 0) {
            return false;
        }
        return info.dwMajorVersion >= 10 && info.dwBuildNumber >= 22000;
    }();

    return cached;
}

} // namespace sys_info
