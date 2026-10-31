#include "platform/borderless.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <utility>
#include <vector>

#include "slint.h"

namespace platform {

namespace {

constexpr UINT_PTR kSubclassId = 1001;
/// 自定义消息：用于把状态更新推迟到当前消息处理结束之后
/// Custom message used to defer state updates until the current message completes
constexpr UINT kDeferredActionMessage = WM_APP + 0x51;

/// 根据窗口 DPI 获取系统标准拉伸边框宽度（SM_CXSIZEFRAME + SM_CXPADDEDBORDER）
/// Standard resize border width for the window DPI
int get_resize_border_width(HWND hwnd)
{
    const UINT dpi = ::GetDpiForWindow(hwnd);
    if (dpi != 0) {
        return ::GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi)
            + ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    }
    return 8;
}

/// 让 DWM 渲染无边框窗口的原生阴影（1px 底部延伸触发）
/// Make DWM render the native shadow for an undecorated window (1px bottom extension)
void extend_frame_for_shadow(HWND hwnd, bool enabled)
{
    MARGINS margins{};
    if (enabled) {
        margins.cxLeftWidth = 0;
        margins.cxRightWidth = 0;
        margins.cyTopHeight = 0;
        margins.cyBottomHeight = 1;
    }
    const HRESULT hr = ::DwmExtendFrameIntoClientArea(hwnd, &margins);
    if (FAILED(hr)) {
        std::printf("[WindowFrame] DwmExtendFrameIntoClientArea failed: 0x%08lX\n",
                    static_cast<unsigned long>(hr));
    }
}

} // namespace

// ────────────────────────────────────────────────────────────────────────────
// 框架内部状态 / Internal frame state
// ────────────────────────────────────────────────────────────────────────────
struct WindowFrame::State {
    WindowAccessor accessor;
    std::shared_ptr<TitlebarAdapter> adapter;
    std::function<void(bool)> on_maximized;

    /// 当前按下的标题栏按钮命中码（HTMINBUTTON / HTMAXBUTTON / HTCLOSE）
    /// Hit code of the currently pressed caption button
    std::optional<WPARAM> pressed_hit;

    HWND hwnd = nullptr;
    bool subclassed = false;

    /// 待执行的状态更新（在 kDeferredActionMessage 中批量执行）
    /// Pending state updates (drained in kDeferredActionMessage)
    std::vector<std::function<void()>> pending;

    slint::Window* window() const { return accessor ? accessor() : nullptr; }

    void post(HWND target, std::function<void()> action)
    {
        if (target == nullptr) {
            return;
        }
        pending.push_back(std::move(action));
        if (!::PostMessageW(target, kDeferredActionMessage, 0, 0)) {
            // 投递失败（窗口尚未进入消息循环）：直接执行，保证状态不丢失
            // Post failed (no message loop yet): run inline so the state is not lost
            drain();
        }
    }

    void drain()
    {
        auto actions = std::move(pending);
        pending.clear();
        for (auto& action : actions) {
            if (action) {
                action();
            }
        }
    }
};

namespace {

using FrameState = WindowFrame::State;

/// 子类化过程中保存状态所有权的容器（对应 Rust 的 `Arc::into_raw` / `Arc::from_raw`）
/// Container keeping the state alive across the subclassing lifetime
/// (the Rust `Arc::into_raw` / `Arc::from_raw` equivalent)
struct StateHolder {
    std::shared_ptr<FrameState> state;
};

/// 命中测试：标题栏控制按钮（使 Win11 Snap Layouts 生效）
/// Hit-test the caption buttons so Win11 Snap Layouts works
std::optional<LRESULT> hit_test_caption_buttons(FrameState& state,
                                               HWND hwnd,
                                               const RECT& rect,
                                               int x,
                                               int y,
                                               bool is_zoomed)
{
    if (!state.adapter) {
        return std::nullopt;
    }
    const TitlebarMetrics metrics = state.adapter->metrics();

    const UINT dpi = ::GetDpiForWindow(hwnd);
    const float scale = dpi == 0 ? 1.0f : static_cast<float>(dpi) / 96.0f;

    const int title_h = static_cast<int>(metrics.titlebar_height * scale);
    const int close_w = static_cast<int>(metrics.close_width * scale);
    const int max_w = static_cast<int>(metrics.maximize_width * scale);
    const int min_w = static_cast<int>(metrics.minimize_width * scale);

    (void)is_zoomed;
    const int btn_top = rect.top;
    const int btn_bottom = rect.top + title_h;

    if (y < btn_top || y >= btn_bottom) {
        return std::nullopt;
    }

    // 关闭按钮 / Close
    if (metrics.show_close) {
        const int close_left = rect.right - close_w;
        if (x >= close_left && x < rect.right) {
            return HTCLOSE;
        }
    }
    const int effective_close_w = metrics.show_close ? close_w : 0;

    // 最大化按钮 / Maximize
    if (metrics.show_maximize) {
        const int max_right = rect.right - effective_close_w;
        const int max_left = max_right - max_w;
        if (x >= max_left && x < max_right) {
            return HTMAXBUTTON;
        }
    }
    const int effective_max_w = metrics.show_maximize ? max_w : 0;

    // 最小化按钮 / Minimize
    if (metrics.show_minimize) {
        const int min_right = rect.right - effective_close_w - effective_max_w;
        const int min_left = min_right - min_w;
        if (x >= min_left && x < min_right) {
            return HTMINBUTTON;
        }
    }

    return std::nullopt;
}

/// 命中测试：窗口 8 方向拉伸边缘 / 8-direction resize borders
std::optional<LRESULT> hit_test_resize_border(HWND hwnd, const RECT& rect, int x, int y)
{
    const int border_width = get_resize_border_width(hwnd);
    const bool left = x - rect.left < border_width;
    const bool right = rect.right - x <= border_width;
    const bool top = y - rect.top < border_width;
    const bool bottom = rect.bottom - y <= border_width;

    if (!(top || bottom || left || right)) {
        return std::nullopt;
    }

    if (top && left) {
        return HTTOPLEFT;
    }
    if (top && right) {
        return HTTOPRIGHT;
    }
    if (bottom && left) {
        return HTBOTTOMLEFT;
    }
    if (bottom && right) {
        return HTBOTTOMRIGHT;
    }
    if (top) {
        return HTTOP;
    }
    if (bottom) {
        return HTBOTTOM;
    }
    if (left) {
        return HTLEFT;
    }
    return HTRIGHT;
}

bool is_caption_button_hit(WPARAM hit)
{
    return hit == HTMINBUTTON || hit == HTMAXBUTTON || hit == HTCLOSE;
}

/// 核心 Win32 子类化消息过程 / Core subclass message handler
LRESULT CALLBACK frame_proc(HWND hwnd,
                            UINT msg,
                            WPARAM wparam,
                            LPARAM lparam,
                            UINT_PTR /*uid_subclass*/,
                            DWORD_PTR ref_data)
{
    auto* holder = reinterpret_cast<StateHolder*>(ref_data);
    FrameState* state = holder != nullptr ? holder->state.get() : nullptr;

    switch (msg) {
    case kDeferredActionMessage: {
        if (state != nullptr) {
            state->drain();
        }
        return 0;
    }

    // 窗口激活 / 失焦 / Window activation
    case WM_ACTIVATE: {
        const bool is_active = LOWORD(wparam) != WA_INACTIVE;
        if (state != nullptr && state->adapter) {
            auto* raw_state = state;
            state->post(hwnd, [raw_state, is_active] {
                raw_state->adapter->set_active(is_active);
                if (!is_active) {
                    raw_state->adapter->set_hover(false, false, false);
                    raw_state->adapter->set_pressed(false, false, false);
                }
            });
        }
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    // 非客户区激活：合成开启时交给系统，否则直接返回 TRUE 避免闪白
    // Non-client activation: let DWM handle it when composition is enabled
    case WM_NCACTIVATE: {
        BOOL composition_enabled = FALSE;
        ::DwmIsCompositionEnabled(&composition_enabled);
        if (composition_enabled) {
            return ::DefSubclassProc(hwnd, msg, wparam, lparam);
        }
        return TRUE;
    }

    // 最大化 / 还原状态变化 / Maximize / restore
    case WM_SIZE: {
        const UINT size_type = static_cast<UINT>(wparam);
        std::optional<bool> is_maximized;
        if (size_type == SIZE_MAXIMIZED) {
            is_maximized = true;
        } else if (size_type == SIZE_RESTORED) {
            is_maximized = false;
        }

        if (is_maximized.has_value() && state != nullptr && state->on_maximized) {
            auto* raw_state = state;
            const bool value = *is_maximized;
            state->post(hwnd, [raw_state, value] {
                if (raw_state->on_maximized) {
                    raw_state->on_maximized(value);
                }
            });
        }
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    // 窗口销毁：注销子类化并释放状态 / Window destroyed: unhook and free state
    case WM_NCDESTROY: {
        const LRESULT result = ::DefSubclassProc(hwnd, msg, wparam, lparam);
        ::RemoveWindowSubclass(hwnd, frame_proc, kSubclassId);
        if (state != nullptr) {
            state->subclassed = false;
        }
        delete holder;
        return result;
    }

    // 非客户区大小计算：最大化时排除任务栏 / Exclude the taskbar when maximized
    case WM_NCCALCSIZE: {
        if (wparam == 0) {
            break;
        }
        if (::IsZoomed(hwnd)) {
            const HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitor_info{};
            monitor_info.cbSize = sizeof(monitor_info);
            if (::GetMonitorInfoW(monitor, &monitor_info)) {
                auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
                params->rgrc[0] = monitor_info.rcWork;
            }
        }
        return 0;
    }

    // 非客户区命中测试：标题栏按钮 + 边缘拉伸 / Non-client hit testing
    case WM_NCHITTEST: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);

        RECT rect{};
        if (!::GetWindowRect(hwnd, &rect)) {
            break;
        }

        const bool is_zoomed = ::IsZoomed(hwnd) != FALSE;
        if (is_zoomed) {
            const HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
            MONITORINFO monitor_info{};
            monitor_info.cbSize = sizeof(monitor_info);
            if (::GetMonitorInfoW(monitor, &monitor_info)) {
                rect = monitor_info.rcWork;
            }
        }

        // 1. 优先判定标题栏控制按钮（Snap Layouts 依赖此处的 HTMAXBUTTON）
        //    Caption buttons first (Snap Layouts relies on HTMAXBUTTON here)
        if (state != nullptr) {
            if (auto hit = hit_test_caption_buttons(*state, hwnd, rect, x, y, is_zoomed)) {
                return *hit;
            }
        }

        // 2. 窗口边缘拉伸（仅窗口化时）
        //    Resize borders (windowed mode only)
        if (!is_zoomed) {
            if (auto hit = hit_test_resize_border(hwnd, rect, x, y)) {
                return *hit;
            }
        }

        // 3. 客户区（交给 Slint 处理，标题栏拖拽由 TouchArea 驱动）
        //    Client area (Slint handles it; the titlebar drag is driven by TouchArea)
        return HTCLIENT;
    }

    // 非客户区鼠标移动 / Non-client mouse move
    case WM_NCMOUSEMOVE: {
        if (state != nullptr && state->adapter) {
            auto* raw_state = state;
            const WPARAM hit = wparam;
            const std::optional<WPARAM> pressed = state->pressed_hit;
            state->post(hwnd, [raw_state, hit, pressed] {
                if (pressed.has_value()) {
                    if (hit == *pressed) {
                        raw_state->adapter->set_pressed(*pressed == HTMINBUTTON,
                                                        *pressed == HTMAXBUTTON,
                                                        *pressed == HTCLOSE);
                        raw_state->adapter->set_hover(false, false, false);
                    } else {
                        raw_state->adapter->set_pressed(false, false, false);
                        raw_state->adapter->set_hover(false, false, false);
                    }
                } else {
                    raw_state->adapter->set_hover(hit == HTMINBUTTON,
                                                  hit == HTMAXBUTTON,
                                                  hit == HTCLOSE);
                    raw_state->adapter->set_pressed(false, false, false);
                }
            });
        }

        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_NONCLIENT | TME_LEAVE;
        tme.hwndTrack = hwnd;
        tme.dwHoverTime = 0;
        ::TrackMouseEvent(&tme);

        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    // 鼠标离开非客户区（或客户区移动）/ Mouse left the non-client area
    case WM_NCMOUSELEAVE:
    case WM_MOUSEMOVE: {
        if (state != nullptr && state->adapter) {
            auto* raw_state = state;
            state->post(hwnd, [raw_state] {
                raw_state->adapter->set_hover(false, false, false);
                raw_state->adapter->set_pressed(false, false, false);
            });
        }
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    // 标题栏按钮按下 / Caption button pressed
    case WM_NCLBUTTONDOWN: {
        if (!is_caption_button_hit(wparam)) {
            break;
        }
        if (state != nullptr) {
            state->pressed_hit = wparam;
            if (state->adapter) {
                auto* raw_state = state;
                const WPARAM hit = wparam;
                state->post(hwnd, [raw_state, hit] {
                    raw_state->adapter->set_pressed(hit == HTMINBUTTON,
                                                    hit == HTMAXBUTTON,
                                                    hit == HTCLOSE);
                    raw_state->adapter->set_hover(false, false, false);
                });
            }
        }
        return 0;
    }

    // 标题栏按钮释放 / Caption button released
    case WM_NCLBUTTONUP:
    case WM_LBUTTONUP: {
        const WPARAM hit = wparam;
        if (state != nullptr) {
            const std::optional<WPARAM> pressed = state->pressed_hit;
            state->pressed_hit.reset();

            auto* raw_state = state;
            if (state->adapter) {
                // 捕获 adapter 与命中码，动作在消息队列中执行，避免在窗口过程中重入
                // Capture the adapter and hit code; run the action from the message queue
                // to avoid re-entering the window proc
                auto adapter = state->adapter;
                (void)raw_state;
                state->post(hwnd, [raw_state, adapter, hit, pressed] {
                    adapter->set_pressed(false, false, false);
                    adapter->set_hover(hit == HTMINBUTTON, hit == HTMAXBUTTON, hit == HTCLOSE);

                    if (pressed.has_value() && *pressed == hit) {
                        WindowFrame frame(raw_state->accessor, adapter);
                        if (hit == HTMINBUTTON) {
                            adapter->on_minimize_clicked(frame);
                        } else if (hit == HTMAXBUTTON) {
                            adapter->on_maximize_clicked(frame);
                        } else if (hit == HTCLOSE) {
                            adapter->on_close_clicked(frame);
                        }
                    }
                });
            }
        }

        if (is_caption_button_hit(hit)) {
            return 0;
        }
        break;
    }

    // 取消模式 / Cancel mode
    case WM_CANCELMODE: {
        if (state != nullptr) {
            state->pressed_hit.reset();
            if (state->adapter) {
                auto* raw_state = state;
                state->post(hwnd, [raw_state] {
                    raw_state->adapter->set_hover(false, false, false);
                    raw_state->adapter->set_pressed(false, false, false);
                });
            }
        }
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    default:
        break;
    }

    return ::DefSubclassProc(hwnd, msg, wparam, lparam);
}

} // namespace

// ────────────────────────────────────────────────────────────────────────────
// TitlebarAdapter 默认实现 / Default adapter behaviour
// ────────────────────────────────────────────────────────────────────────────
void TitlebarAdapter::on_minimize_clicked(WindowFrame& frame)
{
    frame.minimize();
}

void TitlebarAdapter::on_maximize_clicked(WindowFrame& frame)
{
    frame.toggle_maximized();
}

void TitlebarAdapter::on_close_clicked(WindowFrame& frame)
{
    frame.close();
}

// ────────────────────────────────────────────────────────────────────────────
// WindowFrame 实现 / WindowFrame implementation
// ────────────────────────────────────────────────────────────────────────────
WindowFrame::WindowFrame(WindowAccessor accessor, std::shared_ptr<TitlebarAdapter> adapter)
    : state_(std::make_shared<State>())
{
    state_->accessor = std::move(accessor);
    state_->adapter = std::move(adapter);
}

void WindowFrame::with_window(const std::function<void(slint::Window&)>& f)
{
    if (!state_) {
        return;
    }
    if (slint::Window* window = state_->window()) {
        f(*window);
    }
}

void WindowFrame::maximize(bool is_maximized)
{
    with_window([is_maximized](slint::Window& w) { w.set_maximized(is_maximized); });
}

void WindowFrame::toggle_maximized()
{
    with_window([](slint::Window& w) { w.set_maximized(!w.is_maximized()); });
}

void WindowFrame::minimize()
{
    with_window([](slint::Window& w) { w.set_minimized(true); });
}

void WindowFrame::close()
{
    with_window([](slint::Window& w) { w.hide(); });
}

void WindowFrame::drag()
{
    HWND target = hwnd();
    if (target == nullptr) {
        // HWND 尚未就绪时回退到 win32_hwnd()
        // Fall back to win32_hwnd() when the HWND is not cached yet
        with_window([&target](slint::Window& w) { target = w.win32_hwnd(); });
    }
    if (target == nullptr) {
        return;
    }

    // 原生拖拽：释放鼠标捕获后把标题栏按下消息发给自己，进入系统移动循环
    // Native drag: release the capture and send a caption button-down to enter
    // the system move loop (Aero Snap works as with a real titlebar)
    ::ReleaseCapture();
    ::SendMessageW(target, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void WindowFrame::set_undecorated_shadow(bool enabled)
{
    HWND target = hwnd();
    if (target == nullptr) {
        with_window([&target](slint::Window& w) { target = w.win32_hwnd(); });
    }
    if (target != nullptr) {
        extend_frame_for_shadow(target, enabled);
    }
}

void WindowFrame::on_maximized_changed(std::function<void(bool)> callback)
{
    if (state_) {
        state_->on_maximized = std::move(callback);
    }
}

HWND WindowFrame::hwnd() const
{
    return state_ ? state_->hwnd : nullptr;
}

void WindowFrame::apply()
{
    if (!state_) {
        return;
    }
    HWND target = state_->hwnd;
    if (target == nullptr) {
        with_window([&target](slint::Window& w) { target = w.win32_hwnd(); });
    }
    if (target == nullptr) {
        std::printf("[WindowFrame] 无法从 Slint 窗口获取 HWND / Failed to get HWND\n");
        return;
    }

    extend_frame_for_shadow(target, true);
    apply_to_hwnd(target);
}

void WindowFrame::apply_to_hwnd(HWND hwnd_target)
{
    if (!state_ || hwnd_target == nullptr) {
        return;
    }
    state_->hwnd = hwnd_target;

    // 无边框窗口阴影修复（所有 Windows 版本均需，否则 DWM 不渲染阴影）
    // Undecorated shadow fix (required on all Windows versions)
    extend_frame_for_shadow(hwnd_target, true);

    // 安装自定义窗口过程（子类化）/ Install the custom window proc (subclassing)
    if (!state_->subclassed) {
        auto* holder = new StateHolder{state_};
        if (!::SetWindowSubclass(hwnd_target, frame_proc, kSubclassId,
                                 reinterpret_cast<DWORD_PTR>(holder))) {
            std::printf("[WindowFrame] SetWindowSubclass(custom frame) failed\n");
            delete holder;
            return;
        }
        // 注意：holder 由 WM_NCDESTROY 释放 / The holder is released in WM_NCDESTROY
        state_->subclassed = true;
    }

    // 触发 SWP_FRAMECHANGED 通知 DWM 刷新非客户区 metrics 与 Snap Layouts 缓存
    // Notify DWM to refresh NC metrics and the Snap Layouts cache
    ::SetWindowPos(hwnd_target,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
}

} // namespace platform
