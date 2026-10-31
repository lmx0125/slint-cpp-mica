#include "platform/attributes.h"

#include <dwmapi.h>

#include <cstdio>
#include <cstring>

namespace platform {

namespace {

// ────────────────────────────────────────────────────────────────────────────
// DWM 常量定义（部分在旧版 SDK 中可能缺失，这里显式定义）
// DWM constants (may be missing in older SDKs, so defined explicitly here)
// ────────────────────────────────────────────────────────────────────────────
constexpr uint32_t kDwmsbtNone = 1;
constexpr uint32_t kDwmsbtMainWindow = 2;      // Mica
constexpr uint32_t kDwmsbtTransientWindow = 3; // Acrylic
constexpr uint32_t kDwmsbtTabbedWindow = 4;    // Tabbed

/// 未文档化的 DWMWA_MICA_EFFECT（适用于 Win11 22000-22522）
/// Undocumented DWMWA_MICA_EFFECT (Win11 22000-22522)
constexpr uint32_t kDwmwaMicaEffect = 1029;
/// DWMWA_BORDER_COLOR (Win11 build 22000+)
constexpr uint32_t kDwmwaBorderColor = 34;
/// DWMWA_CAPTION_COLOR (Win11 build 22000+)
constexpr uint32_t kDwmwaCaptionColor = 35;
/// DWMWA_TEXT_COLOR (Win11 build 22000+)
constexpr uint32_t kDwmwaTextColor = 36;

const char* backdrop_name(DwmBackdrop b)
{
    switch (b) {
    case DwmBackdrop::Mica:
        return "Mica";
    case DwmBackdrop::Acrylic:
        return "Acrylic";
    case DwmBackdrop::Tabbed:
        return "Tabbed";
    case DwmBackdrop::None:
        return "None";
    }
    return "?";
}

const char* corner_name(CornerPreference c)
{
    switch (c) {
    case CornerPreference::Round:
        return "Round";
    case CornerPreference::RoundSmall:
        return "RoundSmall";
    case CornerPreference::DoNotRound:
        return "DoNotRound";
    }
    return "?";
}

} // namespace

// ────────────────────────────────────────────────────────────────────────────
// 系统深浅色模式检测 / System dark mode detection
// ────────────────────────────────────────────────────────────────────────────
bool is_system_dark_mode()
{
    HKEY hkey = nullptr;
    const LONG status = ::RegOpenKeyExW(
        HKEY_CURRENT_USER,
        L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        0,
        KEY_READ,
        &hkey);

    if (status != ERROR_SUCCESS) {
        std::printf("[WindowAttributes] 无法打开注册表键，默认使用深色模式 (error: %ld)\n",
                    status);
        return true;
    }

    DWORD data = 1;
    DWORD data_size = sizeof(data);
    DWORD data_type = 0;
    const LONG query_status = ::RegQueryValueExW(hkey,
                                                 L"AppsUseLightTheme",
                                                 nullptr,
                                                 &data_type,
                                                 reinterpret_cast<LPBYTE>(&data),
                                                 &data_size);
    ::RegCloseKey(hkey);

    if (query_status != ERROR_SUCCESS || data_type != REG_DWORD) {
        std::printf("[WindowAttributes] 无法读取 AppsUseLightTheme，默认使用深色模式 (error: %ld)\n",
                    query_status);
        return true;
    }

    const bool is_dark = data == 0;
    std::printf("[WindowAttributes] 系统主题检测: AppsUseLightTheme=%lu, is_dark=%d\n",
                static_cast<unsigned long>(data),
                static_cast<int>(is_dark));
    return is_dark;
}

// ────────────────────────────────────────────────────────────────────────────
// Builder 实现 / Builder implementation
// ────────────────────────────────────────────────────────────────────────────
WindowAttributes& WindowAttributes::with_mica()
{
    backdrop_ = DwmBackdrop::Mica;
    return *this;
}

WindowAttributes& WindowAttributes::with_acrylic()
{
    backdrop_ = DwmBackdrop::Acrylic;
    return *this;
}

WindowAttributes& WindowAttributes::with_tabbed()
{
    backdrop_ = DwmBackdrop::Tabbed;
    return *this;
}

WindowAttributes& WindowAttributes::with_backdrop(DwmBackdrop backdrop)
{
    backdrop_ = backdrop;
    return *this;
}

WindowAttributes& WindowAttributes::with_dark_mode(bool enabled)
{
    dark_mode_ = enabled;
    return *this;
}

WindowAttributes& WindowAttributes::with_corner(CornerPreference pref)
{
    corner_ = pref;
    return *this;
}

WindowAttributes& WindowAttributes::with_border_color(uint32_t color_bgr)
{
    border_color_ = color_bgr;
    return *this;
}

WindowAttributes& WindowAttributes::with_caption_color(uint32_t color_bgr)
{
    caption_color_ = color_bgr;
    return *this;
}

WindowAttributes& WindowAttributes::with_text_color(uint32_t color_bgr)
{
    text_color_ = color_bgr;
    return *this;
}

WindowAttributes& WindowAttributes::with_tool_window(bool enabled)
{
    if (enabled) {
        ex_style_add_ |= WS_EX_TOOLWINDOW;
        ex_style_remove_ |= WS_EX_APPWINDOW;
    } else {
        ex_style_remove_ |= WS_EX_TOOLWINDOW;
    }
    return *this;
}

WindowAttributes& WindowAttributes::with_app_window(bool enabled)
{
    if (enabled) {
        ex_style_add_ |= WS_EX_APPWINDOW;
        ex_style_remove_ |= WS_EX_TOOLWINDOW;
    } else {
        ex_style_remove_ |= WS_EX_APPWINDOW;
    }
    return *this;
}

WindowAttributes& WindowAttributes::with_always_on_top(bool enabled)
{
    if (enabled) {
        ex_style_add_ |= WS_EX_TOPMOST;
    } else {
        ex_style_remove_ |= WS_EX_TOPMOST;
    }
    return *this;
}

WindowAttributes& WindowAttributes::with_click_through(bool enabled)
{
    if (enabled) {
        ex_style_add_ |= WS_EX_TRANSPARENT;
    } else {
        ex_style_remove_ |= WS_EX_TRANSPARENT;
    }
    return *this;
}

WindowAttributes& WindowAttributes::with_no_activate(bool enabled)
{
    if (enabled) {
        ex_style_add_ |= WS_EX_NOACTIVATE;
    } else {
        ex_style_remove_ |= WS_EX_NOACTIVATE;
    }
    return *this;
}

WindowAttributes& WindowAttributes::with_raw_style(DWORD add, DWORD remove)
{
    style_add_ |= add;
    style_remove_ |= remove;
    return *this;
}

WindowAttributes& WindowAttributes::with_raw_ex_style(DWORD add, DWORD remove)
{
    ex_style_add_ |= add;
    ex_style_remove_ |= remove;
    return *this;
}

WindowAttributes& WindowAttributes::with_raw_dwm_attribute(uint32_t attr,
                                                          const void* data,
                                                          size_t size)
{
    std::vector<uint8_t> bytes(size);
    if (size > 0 && data != nullptr) {
        std::memcpy(bytes.data(), data, size);
    }
    raw_dwm_attrs_.emplace_back(attr, std::move(bytes));
    return *this;
}

WindowAttributes& WindowAttributes::with_custom_action(std::function<void(HWND)> action)
{
    custom_actions_.push_back(std::move(action));
    return *this;
}

// ────────────────────────────────────────────────────────────────────────────
// 应用属性 / Apply attributes
// ────────────────────────────────────────────────────────────────────────────
void WindowAttributes::apply(HWND hwnd) const
{
    if (hwnd == nullptr) {
        return;
    }

    // 1. 应用 GWL_STYLE
    if (style_add_ != 0 || style_remove_ != 0) {
        const auto current = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_STYLE));
        const DWORD updated = (current | style_add_) & ~style_remove_;
        if (updated != current) {
            ::SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(updated));
        }
    }

    // 2. 应用 GWL_EXSTYLE
    if (ex_style_add_ != 0 || ex_style_remove_ != 0) {
        const auto current = static_cast<DWORD>(::GetWindowLongPtrW(hwnd, GWL_EXSTYLE));
        const DWORD updated = (current | ex_style_add_) & ~ex_style_remove_;
        if (updated != current) {
            ::SetWindowLongPtrW(hwnd, GWL_EXSTYLE, static_cast<LONG_PTR>(updated));
            // 刷新窗口框架使样式生效 / Refresh the frame so the styles take effect
            ::SetWindowPos(hwnd,
                           nullptr,
                           0,
                           0,
                           0,
                           0,
                           SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED
                               | SWP_NOACTIVATE);
        }
    }

    // 3. DWM Backdrop (Mica / Acrylic / Tabbed)
    if (backdrop_.has_value()) {
        uint32_t backdrop_type = kDwmsbtNone;
        switch (*backdrop_) {
        case DwmBackdrop::Mica:
            backdrop_type = kDwmsbtMainWindow;
            break;
        case DwmBackdrop::Acrylic:
            backdrop_type = kDwmsbtTransientWindow;
            break;
        case DwmBackdrop::Tabbed:
            backdrop_type = kDwmsbtTabbedWindow;
            break;
        case DwmBackdrop::None:
            backdrop_type = kDwmsbtNone;
            break;
        }

        const HRESULT hr = ::DwmSetWindowAttribute(
            hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop_type, sizeof(backdrop_type));

        if (FAILED(hr) && *backdrop_ == DwmBackdrop::Mica) {
            // 旧版 Win11（22000-22522）回退到未文档化的 MICA_EFFECT
            // Fall back to the undocumented MICA_EFFECT on early Win11 builds
            const uint32_t enabled = 1;
            ::DwmSetWindowAttribute(hwnd, kDwmwaMicaEffect, &enabled, sizeof(enabled));
        }
        std::printf("[WindowAttributes] 注入 Backdrop(%s): HWND(%p)\n",
                    backdrop_name(*backdrop_),
                    static_cast<void*>(hwnd));
    }

    // 4. 暗色模式 / Dark mode
    if (dark_mode_.has_value()) {
        const uint32_t value = *dark_mode_ ? 1u : 0u;
        ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &value, sizeof(value));
        std::printf("[WindowAttributes] 注入 DarkMode(%d): HWND(%p)\n",
                    static_cast<int>(*dark_mode_),
                    static_cast<void*>(hwnd));
    }

    // 5. 圆角偏好 / Corner preference
    if (corner_.has_value()) {
        uint32_t pref = DWMWCP_ROUND;
        switch (*corner_) {
        case CornerPreference::Round:
            pref = DWMWCP_ROUND;
            break;
        case CornerPreference::RoundSmall:
            pref = DWMWCP_ROUNDSMALL;
            break;
        case CornerPreference::DoNotRound:
            pref = DWMWCP_DONOTROUND;
            break;
        }
        ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
        std::printf("[WindowAttributes] 注入 Corner(%s): HWND(%p)\n",
                    corner_name(*corner_),
                    static_cast<void*>(hwnd));
    }

    // 6. 边框 / 标题 / 文字颜色
    if (border_color_.has_value()) {
        const uint32_t color = *border_color_;
        ::DwmSetWindowAttribute(hwnd, kDwmwaBorderColor, &color, sizeof(color));
    }
    if (caption_color_.has_value()) {
        const uint32_t color = *caption_color_;
        ::DwmSetWindowAttribute(hwnd, kDwmwaCaptionColor, &color, sizeof(color));
    }
    if (text_color_.has_value()) {
        const uint32_t color = *text_color_;
        ::DwmSetWindowAttribute(hwnd, kDwmwaTextColor, &color, sizeof(color));
    }

    // 7. 自定义 DWM 属性注入（逃生通道 1）
    for (const auto& [attr, data] : raw_dwm_attrs_) {
        ::DwmSetWindowAttribute(hwnd, attr, data.data(), static_cast<DWORD>(data.size()));
    }

    // 8. 自定义动作执行（逃生通道 2）
    for (const auto& action : custom_actions_) {
        if (action) {
            action(hwnd);
        }
    }
}

} // namespace platform
