#include "platform/hook.h"

#include <cstdio>
#include <utility>

namespace platform {

namespace {

struct HookContext {
    /// 捕获到 UI HWND 后执行的回调（逃生通道）/ Callback executed with the captured HWND
    std::function<void(HWND)> on_hwnd_ready;
    HHOOK hook = nullptr;
    /// 已识别的目标 UI 窗口 HWND（在 HCBT_CREATEWND 中设置）
    HWND target = nullptr;
    /// 目标窗口标题（可选）/ Optional target window title
    std::optional<std::wstring> target_title;
    /// 回调是否已执行（在 HCBT_ACTIVATE 中设置）/ Whether the callback already ran
    bool applied = false;
};

// 线程局部上下文：CBT Hook 是线程作用域的
// Thread-local context: the CBT hook is thread scoped
thread_local std::optional<HookContext> g_active_context;

bool is_top_level_window(HWND hwnd, const CBT_CREATEWNDW& create_struct)
{
    const auto* cs = create_struct.lpcs;
    if (cs == nullptr) {
        return false;
    }
    const bool is_child = (cs->style & WS_CHILD) != 0;
    const HWND parent = ::GetParent(hwnd);
    const HWND owner = ::GetWindow(hwnd, GW_OWNER);
    return !is_child && parent == nullptr && owner == nullptr;
}

std::wstring create_struct_title(const CBT_CREATEWNDW& create_struct)
{
    const auto* cs = create_struct.lpcs;
    if (cs == nullptr || cs->lpszName == nullptr) {
        return {};
    }
    return std::wstring(cs->lpszName);
}

std::string to_utf8(const std::wstring& wide)
{
    if (wide.empty()) {
        return {};
    }
    const int size = ::WideCharToMultiByte(
        CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(size), '\0');
    ::WideCharToMultiByte(
        CP_UTF8, 0, wide.c_str(), static_cast<int>(wide.size()), out.data(), size, nullptr, nullptr);
    return out;
}

LRESULT CALLBACK cbt_proc(int code, WPARAM wparam, LPARAM lparam)
{
    if (code < 0) {
        return ::CallNextHookEx(nullptr, code, wparam, lparam);
    }

    switch (code) {
    case HCBT_CREATEWND: {
        auto hwnd = reinterpret_cast<HWND>(wparam);
        const auto& create_struct = *reinterpret_cast<const CBT_CREATEWNDW*>(lparam);

        if (!g_active_context.has_value() || !is_top_level_window(hwnd, create_struct)) {
            break;
        }
        auto& ctx = *g_active_context;

        const std::wstring window_name = create_struct_title(create_struct);
        const bool matches =
            !ctx.target_title.has_value() || window_name.find(*ctx.target_title) != std::wstring::npos
            || ctx.target_title->find(window_name) != std::wstring::npos;

        if (matches) {
            ctx.target = hwnd;
            std::printf("[CbtHook] 🎯 HCBT_CREATEWND 捕获到目标 UI 窗口 / Captured target window: "
                        "HWND(%p), Title: '%s'\n",
                        static_cast<void*>(hwnd),
                        to_utf8(window_name).c_str());
        }
        break;
    }

    case HCBT_ACTIVATE: {
        auto hwnd = reinterpret_cast<HWND>(wparam);

        if (!g_active_context.has_value()) {
            break;
        }
        auto& ctx = *g_active_context;
        if (ctx.applied || (ctx.target != nullptr && ctx.target != hwnd)) {
            break;
        }

        ctx.applied = true;
        auto callback = std::move(ctx.on_hwnd_ready);
        HHOOK hook = ctx.hook;

        std::printf("[CbtHook] ⚡ HCBT_ACTIVATE 触发，开始执行 HWND 回调... / Triggered, "
                    "executing HWND callback...\n");
        if (callback) {
            callback(hwnd);
        }
        std::printf("[CbtHook] ✅ 回调完成，卸载 CBT Hook / Callback finished, unhooking CBT "
                    "Hook\n");

        if (hook != nullptr) {
            ::UnhookWindowsHookEx(hook);
        }
        g_active_context.reset();
        break;
    }

    default:
        break;
    }

    return ::CallNextHookEx(nullptr, code, wparam, lparam);
}

} // namespace

CbtHookGuard::CbtHookGuard(CbtHookGuard&& other) noexcept : hook_(std::exchange(other.hook_, nullptr))
{
}

CbtHookGuard& CbtHookGuard::operator=(CbtHookGuard&& other) noexcept
{
    if (this != &other) {
        do_uninstall();
        hook_ = std::exchange(other.hook_, nullptr);
    }
    return *this;
}

CbtHookGuard::~CbtHookGuard()
{
    do_uninstall();
}

std::optional<CbtHookGuard> CbtHookGuard::install(std::optional<std::wstring> target_title,
                                                  std::function<void(HWND)> on_hwnd_ready)
{
    // 清理可能残留的上下文：旧 Hook 若尚未自卸载（例如快速重复点击创建窗口），
    // 先取出句柄并正确卸载，防止 Win32 Hook 句柄泄漏。
    // Clear any residual context so that a hook that has not yet unhooked itself cannot leak.
    if (g_active_context.has_value()) {
        if (g_active_context->hook != nullptr) {
            ::UnhookWindowsHookEx(g_active_context->hook);
        }
        g_active_context.reset();
    }

    HHOOK hook = ::SetWindowsHookExW(WH_CBT, cbt_proc, nullptr, ::GetCurrentThreadId());
    if (hook == nullptr) {
        std::printf("[CbtHook] SetWindowsHookExW(WH_CBT) 失败 / failed, error=%lu\n",
                    static_cast<unsigned long>(::GetLastError()));
        return std::nullopt;
    }

    HookContext ctx;
    ctx.on_hwnd_ready = std::move(on_hwnd_ready);
    ctx.hook = hook;
    ctx.target = nullptr;
    ctx.target_title = std::move(target_title);
    ctx.applied = false;
    g_active_context = std::move(ctx);

    return CbtHookGuard(hook);
}

void CbtHookGuard::uninstall()
{
    do_uninstall();
}

void CbtHookGuard::detach()
{
    // 放弃所有权：Hook 保持安装状态，等待 HCBT_ACTIVATE 自卸载
    // Release ownership: the hook stays installed and unhooks itself in HCBT_ACTIVATE
    hook_ = nullptr;
}

void CbtHookGuard::do_uninstall()
{
    if (hook_ == nullptr) {
        return;
    }
    ::UnhookWindowsHookEx(hook_);
    if (g_active_context.has_value() && g_active_context->hook == hook_) {
        g_active_context.reset();
    }
    hook_ = nullptr;
}

} // namespace platform
