# Windows 100% 毛玻璃高斯虚化（Acrylic Blur）速查与实操手册

> 💡 **面向 Agent / 开发者**：本手册收录了在 **Slint C++ (Win32 / DWM)** 中实现 **100% 硬件双通实时高斯虚化毛玻璃（Acrylic Blur）** 的完整原理、零闪烁注入技术、最简复制模板与避坑指南。下次开发或调用时直接阅读本篇即可快速落地。

---

## 1. 核心概念：为什么是 Acrylic，而不是 Mica？

在 Windows 11 DWM 体系中，许多人容易混淆 **Mica（云母）** 与 **Acrylic（亚克力/毛玻璃）**：

| 特性 | Mica (云母材质, `DWMSBT_MAINWINDOW = 2`) | Acrylic (毛玻璃, `DWMSBT_TRANSIENTWINDOW = 3`) |
| :--- | :--- | :--- |
| **虚化来源** | **仅采样 Windows 桌面壁纸** | **实时采样当前窗口后方的一切画面**（其他程序、桌面、视频、网页） |
| **高斯模糊** | 静态单层壁纸模糊与着色 | **硬件双通 GPU 实时高斯模糊（Dual-Pass Gaussian Blur）** |
| **窗口穿透** | **不穿透窗口！** 后方有其他程序时，看起来就像不透明底色 | **完全穿透！** 移动窗口时，后方所有内容产生实时模糊位移流动 |
| **官方用途** | 应用长期主工作区（减少 GPU 持续开销） | 浮层、临时对话框、侧边栏、极致质感的半透明现代 UI |
| **100% 极透** | 仅显示壁纸的微光纹理 | **100% 呈现后方真实图层的高斯虚化，无任何杂色遮罩阻挡** |

> [!IMPORTANT]
> 要实现用户所说的 **“真正的毛玻璃高斯虚化”**，底层必须启用 **Acrylic**（`DWMSBT_TRANSIENTWINDOW = 3`），同时在 Slint UI 层将背景透明度彻底放开（`alpha = 0.0`），才能达到 **100% 极透穿透**！

---

## 2. 极速落地：一键复制完整模板 (Quick Start)

### 2.1 Slint 侧：100% 极透配方 (`app-window.slint`)

```slint
import { Palette, Button } from "std-widgets.slint";

export component AppWindow inherits Window {
    // 1. 必须声明无边框
    no-frame: true;
    min-width: 600px;
    min-height: 450px;

    // 2. 外部 C++ 注入的激活标志与透明度控制
    in-out property <bool> is-acrylic-active: true;
    in-out property <float> glass-transparency: 100.0; // 100.0 即 100% 极透高斯虚化

    // 3. 根窗口背景：100% 模式下 alpha 为 0，彻底透明，透出 DWM 硬件高斯模糊
    background: self.is-acrylic-active
        ? (Palette.color-scheme == ColorScheme.dark
            ? #000000.with-alpha((100.0 - root.glass-transparency) / 100.0)
            : #ffffff.with-alpha((100.0 - root.glass-transparency) / 100.0))
        : Palette.background;

    // 4. 内容层：使用带微光边框与半透明底色的卡片（避免不透明纯白/纯黑把虚化挡死）
    VerticalLayout {
        padding: 24px;
        spacing: 16px;

        Rectangle {
            border-radius: 8px;
            // 85%~90% 透光的悬浮亚克力面板，保证毛玻璃贯穿卡片
            background: Palette.color-scheme == ColorScheme.dark
                ? #ffffff.with-alpha(0.08)
                : #ffffff.with-alpha(0.40);
            border-width: 1px;
            border-color: Palette.color-scheme == ColorScheme.dark ? #ffffff1a : #00000015;

            VerticalLayout {
                padding: 20px;
                Text {
                    text: "100% 硬件双通高斯虚化毛玻璃已生效";
                    color: Palette.color-scheme == ColorScheme.dark ? #ffffff : #111111;
                    font-size: 16px;
                    font-weight: 700;
                }
            }
        }
    }
}
```

### 2.2 C++ 侧：DWM 硬件属性注入 (`main.cpp`)

```cpp
#include <windows.h>
#include <dwmapi.h>
#pragma comment(lib, "dwmapi.lib")

#ifndef DWMWA_SYSTEMBACKDROP_TYPE
#define DWMWA_SYSTEMBACKDROP_TYPE 38
#endif

// 启用 Win11 官方 Acrylic 硬件双通高斯虚化
void enable_100_acrylic_blur(HWND hwnd, bool dark_mode = true)
{
    // 1. 扩展 DWM 框架进客户区（毛玻璃渲染的几何基础，必须调用）
    MARGINS margins{ 0, 0, 0, 1 };
    ::DwmExtendFrameIntoClientArea(hwnd, &margins);

    // 2. 注入 DWM 系统背景类型：DWMSBT_TRANSIENTWINDOW (3) = Acrylic
    uint32_t backdrop_type = 3; 
    ::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &backdrop_type, sizeof(backdrop_type));

    // 3. 匹配系统深浅色浸入模式
    uint32_t dark = dark_mode ? 1 : 0;
    ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));

    // 4. 设置标准圆角
    uint32_t corner = DWMWCP_ROUND; // 2
    ::DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &corner, sizeof(corner));

    // 5. 刷新窗口样式使 DWM 材质即时生效
    ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                   SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
    ::InvalidateRect(hwnd, nullptr, TRUE);
}
```

---

## 3. 三大底层核心机制解析

### 3.1 零闪烁架构：CBT Hook (`WH_CBT`)

* **为什么普通方案会闪烁？**
  如果使用常规定时器或在 `app->window().show()` 之后才设置 DWM 属性，Win32 会先绘制一帧默认的白色/灰色背景，随后才突变成毛玻璃，产生刺眼的“白闪”。
* **CBT Hook 解决机制**：
  在主窗口创建前安装线程钩子 `SetWindowsHookExW(WH_CBT, ...)`，捕获 `HCBT_CREATEWND`。在 `CreateWindowExW` 返回 HWND 的那一瞬间（**首帧 `WM_PAINT` 尚未到达前**），立即完成 DWM 属性注入与无边框阴影子类化。
* **效果**：窗口首次显现在显示器上的第 0 帧就是纯净的毛玻璃。

### 3.2 动态材质平滑切换（Mica ↔ Acrylic ↔ Tabbed ↔ 纯色）

在本项目中，可以通过 Slint 回调和 Win32 消息实现运行中秒切：

```cpp
app->on_change_dwm_backdrop([app_weak, frame_holder](int index) {
    // index 映射表：
    // 0: Mica 云母 (DWMSBT_MAINWINDOW = 2)
    // 1: Acrylic 双通高斯虚化毛玻璃 (DWMSBT_TRANSIENTWINDOW = 3) -> 100% 推荐
    // 2: Tabbed 深层亚克力 (DWMSBT_TABBEDWINDOW = 4)
    // 3: None 纯色背景 (DWMSBT_NONE = 1)
    uint32_t type = (index == 0) ? 2 : (index == 1) ? 3 : (index == 2) ? 4 : 1;
    HWND hwnd = (*frame_holder)->hwnd();
    if (hwnd) {
        ::DwmSetWindowAttribute(hwnd, DWMWA_SYSTEMBACKDROP_TYPE, &type, sizeof(type));
        ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
        ::InvalidateRect(hwnd, nullptr, TRUE);
    }
});
```

### 3.3 Win10 降级兼容：`SetWindowCompositionAttribute`

在 Windows 10（1803 ~ 21H2）中，微软尚未开放 `DWMWA_SYSTEMBACKDROP_TYPE`。如需向下兼容，可调用 `user32.dll` 的未公开 API 实现超透亚克力：

```cpp
typedef enum _ACCENT_STATE {
    ACCENT_DISABLED = 0,
    ACCENT_ENABLE_GRADIENT = 1,
    ACCENT_ENABLE_TRANSPARENTGRADIENT = 2,
    ACCENT_ENABLE_BLURBEHIND = 3,         // Aero 纯净晶透高斯虚化
    ACCENT_ENABLE_ACRYLICBLURBEHIND = 4,  // Win10 亚克力高斯模糊 (1803+)
} ACCENT_STATE;

typedef struct _ACCENT_POLICY {
    ACCENT_STATE AccentState;
    DWORD AccentFlags;     // 设为 2
    DWORD GradientColor;   // 0x00000000 即 100% 极透无遮罩
    DWORD AnimationId;
} ACCENT_POLICY;

typedef struct _WINCOMPATTRDATA {
    DWORD Attribute; // 19 = WCA_ACCENT_POLICY
    PVOID Data;
    ULONG SizeOfData;
} WINCOMPATTRDATA;

typedef BOOL (WINAPI *pfnSetWindowCompositionAttribute)(HWND, WINCOMPATTRDATA*);

void apply_win10_acrylic(HWND hwnd)
{
    static auto pfn = (pfnSetWindowCompositionAttribute)
        ::GetProcAddress(::GetModuleHandleW(L"user32.dll"), "SetWindowCompositionAttribute");
    if (pfn && hwnd) {
        ACCENT_POLICY policy{ ACCENT_ENABLE_ACRYLICBLURBEHIND, 2, 0x00000000, 0 };
        WINCOMPATTRDATA data{ 19, &policy, sizeof(policy) };
        pfn(hwnd, &data);
    }
}
```

---

## 4. 本仓库相关文件导航 (Codebase Map)

若需在现有工程中排查或调整毛玻璃，可直接定位以下文件：

| 功能 / 模块 | 关键文件 | 作用说明 |
| :--- | :--- | :--- |
| **DWM 属性建造器** | [`src/platform/attributes.cpp`](../src/platform/attributes.cpp) | `with_acrylic()` 将 backdrop 设为 `DwmBackdrop::Acrylic` |
| **主窗口默认属性** | [`src/windows/app/attributes.cpp`](../src/windows/app/attributes.cpp) | 预置配置：默认使用 `.with_acrylic()` |
| **运行时材质切换** | [`src/main.cpp`](../src/main.cpp) | 监听 `on_change_dwm_backdrop`，动态调用 `DwmSetWindowAttribute` |
| **Win10 降级函数** | [`src/main.cpp`](../src/main.cpp) | `apply_accent_policy` 实现 `ACCENT_ENABLE_ACRYLICBLURBEHIND` |
| **无边框阴影扩展** | [`src/platform/borderless.cpp`](../src/platform/borderless.cpp) | `DwmExtendFrameIntoClientArea` 注入底边 1px 阴影 |
| **UI 背景透明度公式** | [`ui/app-window/app-window.slint`](../ui/app-window/app-window.slint) | `#0d0d0d.with-alpha((100.0 - glass-transparency) / 100.0)` |
| **高斯虚化滑动调节** | [`ui/app-window/app-window.slint`](../ui/app-window/app-window.slint) | 滑块动态绑定 `glass-transparency` (50% ~ 100%) |
| **卡片半透明色彩 Token** | [`ui/common/motion.slint`](../ui/common/motion.slint) | `MicaTheme.surface` 动态受 `surface-transparency` 驱动 |

---

## 5. Agent 避坑与故障排查清单 (Troubleshooting)

### ⚠️ 坑 1：NVIDIA 显卡 + OpenGL 后端导致背景死黑 (Black Screen)
* **原因**：NVIDIA 驱动默认策略对含有 Alpha 通道的 OpenGL 分层窗口兼容性较差，可能将透明像素渲染为纯黑。
* **排查/解决**：
  1. 打开 **NVIDIA 控制面板** -> **管理 3D 设置** -> 找到 **OpenGL GDI 兼容性** -> 改为 **“优先兼容性” (Prefer Compatible)**。
  2. 或者切换 Slint 渲染后端为 Skia / Direct3D / Software：
     ```powershell
     $env:SLINT_BACKEND="winit-skia" # 或 winit-software
     ```

### ⚠️ 坑 2：窗口移动或缩放时出现残影、撕裂
* **原因**：更改材质或窗口尺寸后没有触发 DWM 重新合成。
* **解决**：在设置 DWM 属性后，必须调用：
  ```cpp
  ::SetWindowPos(hwnd, nullptr, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED | SWP_NOACTIVATE);
  ::InvalidateRect(hwnd, nullptr, TRUE);
  ```

### ⚠️ 坑 3：文字在 100% 虚化背景上看不清
* **原因**：当透光率拉满到 100% 时，若下层窗口恰好也是文字或白色图片，文字对比度会下降。
* **解决**：
  1. 保证文本颜色绝对高反差（深色模式下用纯白 `#ffffff`，浅色模式用 `#111111`）。
  2. 文字所在容器使用微光磨砂卡片（如 `MicaTheme.surface` 配 80%~85% 透光率），既保留了透底虚化流动感，又构筑了阅读衬底。

### ⚠️ 坑 4：自绘标题栏 Snap Layouts 贴靠菜单失效
* **原因**：DWM 接管的最大化贴靠菜单依赖非客户区消息。
* **解决**：在 Win32 子类化的 `WM_NCHITTEST` 中，检测鼠标悬停在最大化按钮区域时，必须返回 `HTMAXBUTTON`（参见 [`src/platform/borderless.cpp`](../src/platform/borderless.cpp)）。

---

## 6. Agent 下次调用 SOP（Standard Operating Procedure）

当用户发出类似指令：
> “把窗口改成 100% 毛玻璃高斯虚化” 或 “给新建的窗口加上亚克力穿透虚化”

请按以下 4 步无缝执行：

1. **设置 DWM 材质为 Acrylic**：
   在窗口初始化处调用 `attributes.with_acrylic()`，或直接调用 `::DwmSetWindowAttribute(hwnd, 38 /*DWMWA_SYSTEMBACKDROP_TYPE*/, &type, sizeof(type))`（`type = 3`）。
2. **设置 Slint Window 背景透明**：
   在 `.slint` 文件的窗口根节点设置 `no-frame: true;`，并将 `background: transparent;` 或 `background: #000000.with-alpha(0.0);`。
3. **设置面板/卡片透光率**：
   确保窗口内的父级容器没有设置硬编码的不透明底色（如 `#ffffff` 或 `#202020`），统一替换为 `MicaTheme.surface` 或 `with-alpha(0.1 ~ 0.3)`。
4. **编译与验证**：
   ```powershell
   cmake --build cpp/build --config Release
   ```
   拖动窗口越过不同的后台程序（如带有彩色代码的编辑器、网页），观察窗口内背景是否呈现出实时动态模糊流变，确认 100% 虚化生效。
