#include "windows/native_terminal/native_caption.h"

#include <commctrl.h>
#include <dwmapi.h>
#include <windowsx.h>

#include <algorithm>
#include <cstdio>
#include <functional>
#include <memory>
#include <utility>
#include <vector>

namespace windows::native_terminal {

/// 子类化期间共享的状态 / State shared across the subclassing lifetime
/// 自定义消息：把状态更新推迟到当前消息处理结束之后
/// Custom message deferring state updates until the current message completes
constexpr UINT kDeferredActionMessage = WM_APP + 0x52;

struct NativeCaptionFrame::State {
    /// 激活状态变更回调 / Activation-state callback
    std::function<void(bool)> on_active;
    /// 是否已安装子类化 / Whether the subclassing is installed
    bool subclassed = false;
    /// 待执行的状态更新（在 kDeferredActionMessage 中批量执行）
    /// Pending state updates (drained in kDeferredActionMessage)
    std::vector<std::function<void()>> pending;

    void post(HWND target, std::function<void()> action)
    {
        if (target == nullptr) {
            return;
        }
        pending.push_back(std::move(action));
        if (!::PostMessageW(target, kDeferredActionMessage, 0, 0)) {
            // 投递失败（尚未进入消息循环）：直接执行，保证状态不丢失
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

constexpr UINT_PTR kSubclassId = 2001;

/// 子类化过程中保存状态所有权的容器 / Container keeping the state alive
struct StateHolder {
    std::shared_ptr<NativeCaptionFrame::State> state;
};

/// 根据 DPI 获取窗口拉伸边框宽度 / Resize border width for the window DPI
int get_resize_border_width(HWND hwnd)
{
    const UINT dpi = ::GetDpiForWindow(hwnd);
    if (dpi != 0) {
        return ::GetSystemMetricsForDpi(SM_CXSIZEFRAME, dpi)
            + ::GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
    }
    return 8;
}

/// 获取窗口所在显示器的工作区矩形 / Work area of the window's monitor
bool get_monitor_work_area(HWND hwnd, RECT& out)
{
    const HMONITOR monitor = ::MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST);
    MONITORINFO monitor_info{};
    monitor_info.cbSize = sizeof(monitor_info);
    if (!::GetMonitorInfoW(monitor, &monitor_info)) {
        return false;
    }
    out = monitor_info.rcWork;
    return true;
}

/// 简化子类化窗口过程：WM_NCCALCSIZE（扩展客户区）与 WM_NCHITTEST（标题栏与原生按钮）
/// Simplified subclass proc: WM_NCCALCSIZE (extend the client area) and
/// WM_NCHITTEST (titlebar and native buttons)
LRESULT CALLBACK native_caption_proc(HWND hwnd,
                                    UINT msg,
                                    WPARAM wparam,
                                    LPARAM lparam,
                                    UINT_PTR /*uid_subclass*/,
                                    DWORD_PTR ref_data)
{
    auto* holder = reinterpret_cast<StateHolder*>(ref_data);
    NativeCaptionFrame::State* state = holder != nullptr ? holder->state.get() : nullptr;

    // 延迟投递的状态更新 / Deferred state updates
    if (msg == kDeferredActionMessage) {
        if (state != nullptr) {
            state->drain();
        }
        return 0;
    }

    // 窗口激活 / 失焦：推迟到当前消息处理结束之后再写 Slint 属性，
    // 避免在窗口过程里重入 Slint 的属性设置。
    // Window activation: defer writing the Slint property until the current
    // message completes so we never re-enter Slint from inside the window proc.
    if (msg == WM_ACTIVATE) {
        const bool is_active = LOWORD(wparam) != WA_INACTIVE;
        if (state != nullptr && state->on_active) {
            auto* raw_state = state;
            state->post(hwnd, [raw_state, is_active] {
                if (raw_state->on_active) {
                    raw_state->on_active(is_active);
                }
            });
        }
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    // 先让 DWM 处理 hit-test 与按钮交互消息（关键！否则 DWM 不会绘制/响应原生按钮）
    // Let DWM handle hit-testing and button interaction first; otherwise DWM neither
    // paints nor responds to the native buttons
    {
        LRESULT dwm_result = 0;
        if (::DwmDefWindowProc(hwnd, msg, wparam, lparam, &dwm_result)) {
            return dwm_result;
        }
    }

    switch (msg) {
    // 非客户区大小计算：客户区覆盖标题栏，DWM 原生按钮悬浮在客户区之上
    // NCCALCSIZE: extend the client area over the titlebar so the native buttons float
    // on top of it
    case WM_NCCALCSIZE: {
        if (wparam == 0) {
            break;
        }
        auto* params = reinterpret_cast<NCCALCSIZE_PARAMS*>(lparam);
        const LONG original_top = params->rgrc[0].top;

        // 无论窗口化还是最大化，先让 Windows/DWM 计算标准非客户区布局
        // In both windowed and maximized modes, let Windows/DWM compute the standard
        // non-client layout first
        ::DefWindowProcW(hwnd, msg, wparam, lparam);

        // 统一把 top 恢复，使客户区延伸到窗口最顶部（偏移 0）
        // Restore top so the client area reaches the very top of the window
        params->rgrc[0].top = original_top;
        return 0;
    }

    // 非客户区命中测试 / Non-client hit testing
    case WM_NCHITTEST: {
        const int x = GET_X_LPARAM(lparam);
        const int y = GET_Y_LPARAM(lparam);
        const bool is_zoomed = ::IsZoomed(hwnd) != FALSE;

        RECT rect{};
        if (!::GetWindowRect(hwnd, &rect)) {
            return HTCLIENT;
        }

        // 标题栏高度按窗口 DPI 缩放（Slint 的 36px 是逻辑像素）
        // Scale the titlebar height for the window DPI (36px is a logical size)
        const UINT dpi = ::GetDpiForWindow(hwnd);
        const LONG titlebar_height =
            dpi != 0 ? ::MulDiv(kTitlebarHeight, static_cast<int>(dpi), 96) : kTitlebarHeight;

        // 标题栏顶部 / 底部 / 右侧边界（屏幕坐标）
        // Titlebar top / bottom / right bounds in screen coordinates
        LONG btn_top = rect.top;
        LONG btn_bottom = rect.top + titlebar_height;
        LONG btn_right = rect.right;
        if (is_zoomed) {
            RECT work_area{};
            if (get_monitor_work_area(hwnd, work_area)) {
                btn_top = work_area.top;
                btn_bottom = work_area.top + titlebar_height;
                btn_right = work_area.right;
            }
        } else {
            // 非最大化时窗口右侧包含 8px 不可见的拉伸边框，客户区右边缘才是
            // DWM 实际绘制按钮的位置；用 GetWindowRect().right 会让按钮判定区右移。
            // The non-maximized window rect includes the invisible resize border on
            // the right; DWM draws the buttons at the client-area edge, so using
            // GetWindowRect().right would shift the hit zones to the right.
            RECT client{};
            if (::GetClientRect(hwnd, &client)) {
                POINT client_right{client.right, 0};
                if (::ClientToScreen(hwnd, &client_right)) {
                    btn_right = client_right.x;
                }
            }
        }

        // DWMWA_CAPTION_BUTTON_BOUNDS 获取按钮区域宽度。DWM 返回的矩形包含右侧
        // 不可见的拉伸边框（按钮实际按客户区右边缘右对齐），需扣除该宽度才能与
        // 三个原生按钮（各 46 逻辑像素）精确对齐；获取失败时按 3×46 逻辑像素回退。
        // DWM reports the button area including the invisible right resize border;
        // the real buttons are right-aligned to the client edge and 46 logical px
        // each. Subtract the border for precise alignment; fall back to 3×46.
        RECT btn_rect{};
        LONG btn_area_w = ::MulDiv(46 * 3, static_cast<int>(dpi == 0 ? 96 : dpi), 96);
        if (SUCCEEDED(::DwmGetWindowAttribute(
                hwnd, DWMWA_CAPTION_BUTTON_BOUNDS, &btn_rect, sizeof(btn_rect)))
            && btn_rect.right > btn_rect.left) {
            btn_area_w = btn_rect.right - btn_rect.left;
            const LONG excess = get_resize_border_width(hwnd);
            if (btn_area_w > excess) {
                btn_area_w -= excess;
            }
        }
        const LONG btn_left = btn_right - btn_area_w;

        // 1. 优先判定三大原生按钮区域 / Caption buttons first
        if (y >= btn_top && y < btn_bottom && x >= btn_left && x < btn_right) {
            const LONG single_btn_w = std::max<LONG>(btn_area_w / 3, 1);
            const LONG dist_from_right = btn_right - x;
            // 距离从 1 开始（x 位于客户区内），因此边界用 <= 保证每个按钮恰好
            // 覆盖 46 逻辑像素宽，不产生 1px 错位。
            // dist starts at 1 inside the client area; use <= so each button
            // covers exactly its width without a one-pixel shift.
            if (dist_from_right <= single_btn_w) {
                return HTCLOSE;
            }
            if (dist_from_right <= single_btn_w * 2) {
                return HTMAXBUTTON;
            }
            return HTMINBUTTON;
        }

        // 2. 窗口边缘 8 方向拉伸（仅窗口化时）/ 8-direction resize (windowed only)
        if (!is_zoomed) {
            const int border_width = get_resize_border_width(hwnd);
            const bool left = x - rect.left < border_width;
            const bool right = rect.right - x <= border_width;
            const bool top = y - rect.top < border_width;
            const bool bottom = rect.bottom - y <= border_width;

            if (top || bottom || left || right) {
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
        }

        // 3. 标题栏其余区域：HTCAPTION 允许拖拽与双击最大化 / 还原
        // The rest of the titlebar returns HTCAPTION (drag and double-click maximize)
        if (y >= btn_top && y < btn_bottom && x < btn_left) {
            return HTCAPTION;
        }

        return HTCLIENT;
    }

    // 非客户区鼠标移动：注册 TrackMouseEvent 确保悬停高亮能正确刷新与清除
    // Non-client mouse move: register TrackMouseEvent so hover clears on leave
    case WM_NCMOUSEMOVE: {
        TRACKMOUSEEVENT tme{};
        tme.cbSize = sizeof(tme);
        tme.dwFlags = TME_NONCLIENT | TME_LEAVE;
        tme.hwndTrack = hwnd;
        tme.dwHoverTime = 0;
        ::TrackMouseEvent(&tme);
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    // 鼠标离开非客户区：通知 DWM 清理按钮高亮
    // Non-client mouse leave: notify DWM to clear the caption button highlight
    case WM_NCMOUSELEAVE: {
        LRESULT dwm_result = 0;
        ::DwmDefWindowProc(hwnd, msg, wparam, lparam, &dwm_result);
        return ::DefSubclassProc(hwnd, msg, wparam, lparam);
    }

    // 窗口销毁时注销子类化 / Unregister the subclassing on destruction
    case WM_NCDESTROY: {
        std::printf("[NativeCaption] WM_NCDESTROY — 注销子类化 / Unregistered the "
                    "subclassing\n");
        const LRESULT result = ::DefSubclassProc(hwnd, msg, wparam, lparam);
        ::RemoveWindowSubclass(hwnd, native_caption_proc, kSubclassId);
        if (state != nullptr) {
            state->on_active = {};
            state->subclassed = false;
            state->pending.clear();
        }
        delete holder;
        return result;
    }

    default:
        break;
    }

    return ::DefSubclassProc(hwnd, msg, wparam, lparam);
}

} // namespace

void NativeCaptionFrame::drag() const
{
    if (hwnd_ == nullptr) {
        return;
    }
    ::ReleaseCapture();
    ::SendMessageW(hwnd_, WM_NCLBUTTONDOWN, HTCAPTION, 0);
}

void NativeCaptionFrame::toggle_maximized() const
{
    if (hwnd_ == nullptr) {
        return;
    }
    const bool is_zoomed = ::IsZoomed(hwnd_) != FALSE;
    ::SendMessageW(hwnd_, WM_SYSCOMMAND, is_zoomed ? SC_RESTORE : SC_MAXIMIZE, 0);
}

void NativeCaptionFrame::on_active_changed(std::function<void(bool)> callback)
{
    if (!state_) {
        state_ = std::make_shared<State>();
    }
    state_->on_active = std::move(callback);
}

bool NativeCaptionFrame::apply_to_hwnd(HWND hwnd)
{
    if (hwnd == nullptr) {
        return false;
    }
    hwnd_ = hwnd;

    // 1. 把 DWM 材质的边框帧扩展到整个客户区，让 DWM 有空间绘制原生按钮与材质
    // 1. Extend the DWM frame over the whole client area so DWM can paint the native
    //    buttons and the backdrop
    {
        MARGINS margins{};
        margins.cxLeftWidth = -1;
        margins.cxRightWidth = -1;
        margins.cyTopHeight = -1;
        margins.cyBottomHeight = -1;
        const HRESULT hr = ::DwmExtendFrameIntoClientArea(hwnd, &margins);
        if (FAILED(hr)) {
            std::printf("[NativeCaption] DwmExtendFrameIntoClientArea failed: 0x%08lX\n",
                        static_cast<unsigned long>(hr));
            return false;
        }
    }

    // 2. 安装简化子类化（仅处理 WM_NCCALCSIZE / WM_NCHITTEST / WM_NCMOUSE* / WM_NCDESTROY）
    // 2. Install the simplified subclass proc
    if (!state_) {
        state_ = std::make_shared<State>();
    }
    if (!state_->subclassed) {
        auto* holder = new StateHolder{state_};
        if (!::SetWindowSubclass(hwnd,
                                 native_caption_proc,
                                 kSubclassId,
                                 reinterpret_cast<DWORD_PTR>(holder))) {
            delete holder;
            std::printf("[NativeCaption] SetWindowSubclass failed\n");
            return false;
        }
        state_->subclassed = true;
    }

    // 3. 触发 SWP_FRAMECHANGED 通知 DWM 刷新 / Notify DWM to refresh
    ::SetWindowPos(hwnd,
                   nullptr,
                   0,
                   0,
                   0,
                   0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    return true;
}

} // namespace windows::native_terminal
