# Slint + Win32/DWM 无边框 & Mica 模板（C++ 移植版）

本目录是 [slint-windows-mica-template](https://github.com/Secrtect/slint-windows-mica-template)
（Rust 版）的 **C++ 1:1 移植**，使用 Slint C++ API + Win32/DWM 原生调用实现同样的能力。

> 与原项目一致：这是学习/参考用的练习项目，不是生产级库。

---

## ✨ 功能特性

| 能力 | 说明 |
| :--- | :--- |
| **DWM Mica 材质** | `DwmSetWindowAttribute(DWMWA_SYSTEMBACKDROP_TYPE)`，自动回退到旧版 Win11 的 `DWMWA_MICA_EFFECT` |
| **CBT Hook 零闪烁** | `SetWindowsHookExW(WH_CBT)` 在 `CreateWindowExW` 瞬间拿到 HWND，首帧绘制前完成属性注入 + 子类化 |
| **Win11 Snap Layouts** | `WM_NCHITTEST` 返回 `HTMAXBUTTON` / `HTMINBUTTON` / `HTCLOSE`，悬停最大化按钮弹出贴靠菜单 |
| **8 方向拉伸** | DPI 感知的边框宽度（`GetSystemMetricsForDpi`） |
| **多显示器居中** | 跟随鼠标所在显示器，按 `rcWork` 居中并防超屏 |
| **自绘标题栏** | 100% Slint 绘制的最小化 / 最大化 / 关闭按钮，hover / pressed 状态由 Win32 非客户区消息驱动 |
| **DWM 原生按钮** | 原生终端窗口保留 DWM 绘制的标题栏按钮，配合 `WM_NCCALCSIZE` + `DwmDefWindowProc` |
| **关闭行为可选** | `CloseBehavior::Destroy`（默认，释放资源）/ `CloseBehavior::Hide`（保留状态复用） |

## 📂 目录结构

```text
cpp/
├── CMakeLists.txt              # 构建入口
├── cmake/FetchSlintSdk.cmake   # Slint SDK 自动获取（预编译包 / 本地 / 源码构建）
├── third_party/slint-sdk/      # 可选：放置解压好的官方预编译 SDK
├── ui/                         # 与 Rust 版完全相同的 .slint 源码（未改动）
│   ├── app-window/
│   ├── custom-terminal-window/
│   └── native-terminal-window/
└── src/
    ├── main.cpp                # 入口：CBT Hook 编排 + 事件循环
    ├── sys_info.{h,cpp}        # Win11 版本检测（RtlGetVersion）
    ├── platform/               # 通用 Windows 平台层
    │   ├── attributes.{h,cpp}  # 声明式 DWM/Win32 属性 + 逃生通道
    │   ├── borderless.{h,cpp}  # WindowFrame + Win32 子类化 + Snap Layouts 命中测试
    │   ├── controls.h          # TitlebarButtons
    │   ├── display.{h,cpp}     # 多显示器居中
    │   ├── effects.{h,cpp}     # Mica 透明背景联动
    │   └── hook.{h,cpp}        # CbtHookGuard（RAII）
    └── windows/                # 业务窗口
        ├── windows.h           # CloseBehavior
        ├── app/                # 主窗口（自绘标题栏）
        ├── custom_terminal/    # 自绘终端窗口
        └── native_terminal/    # DWM 原生按钮终端窗口
```

`src/windows/*` 与 `ui/*` 一一对应，和 Rust 版保持相同的映射关系。

## 🚀 构建

### 前置条件
- Windows 10/11 x64（Mica 需要 Win11 Build 22000+）
- CMake ≥ 3.21
- Visual Studio 2022（MSVC，C++20）
- **不需要 Rust**（默认使用官方预编译 C++ SDK）

### 一键构建

```bash
cd cpp
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release
./build/Release/slint-windows-mica-template.exe
```

首次配置时 CMake 会按以下顺序获取 Slint C++ SDK（v1.17.1）：

1. `-DSLINT_SDK_DIR=<目录>` 手动指定；
2. `cpp/third_party/slint-sdk/`（若已解压官方预编译包）；
3. 自动下载 `Slint-cpp-1.17.1-win64-MSVC-AMD64.exe` 并静默解压到 `build/_deps/slint-sdk`；
4. `-DSLINT_BUILD_FROM_SOURCE=ON` 走 FetchContent 源码构建（**需要 Rust 工具链**，首次编译较慢）。

构建结束后 `slint_cpp.dll` 会自动复制到可执行文件同目录。

## 🧩 与 Rust 版的对应关系

| Rust | C++ |
| :--- | :--- |
| `platform::attributes::WindowAttributes` | `platform::WindowAttributes`（builder 链式调用） |
| `platform::borderless::WindowFrame<T>` | `platform::WindowFrame`（非模板，通过 `TitlebarAdapter` 类型擦除） |
| `platform::borderless::TitlebarAdapter<T>` | `platform::TitlebarAdapter`（虚函数接口） |
| `platform::hook::CbtHookGuard` | `platform::CbtHookGuard`（`install()` 返回 `std::optional`，`detach()` 对应 `mem::forget`） |
| `platform::display::center_component_on_active_monitor` | `platform::center_on_active_monitor(slint::Window&, w, h)` |
| `platform::effects::apply_mica_effect` | `platform::apply_mica_effect(std::function<void()>, bool)` |
| `winit` 的 `drag_window()` | `ReleaseCapture()` + `WM_NCLBUTTONDOWN(HTCAPTION)` |
| `Arc<Mutex<T>>` 共享状态 | `std::shared_ptr<T>`（全部在 UI 线程） |
| `weak.upgrade_in_event_loop(...)` | 投递自定义消息 `WM_APP+0x51`，在当前消息处理结束后执行 |

Rust 版依靠 `i_slint_backend_winit` 的扩展 trait 拿到 HWND；C++ 版除了 `slint::Window::win32_hwnd()`
之外，主要沿用 **CBT Hook** 在窗口创建瞬间拿到 HWND 的思路，因此两种语言的行为一致。

## 🍳 常见改造

### 1. 隐藏标题栏按钮

```cpp
// src/windows/app/app.cpp 或 custom_terminal.cpp
platform::TitlebarButtons buttons;
buttons.show_minimize = false;   // 不显示最小化
buttons.show_maximize = false;   // 不显示最大化
buttons.show_close = true;
```

隐藏后会同时影响 Win32 命中测试（空白区域不再触发 Snap Layouts）和 Slint UI 按钮可见性。

### 2. 切换材质 / 置顶 / 工具窗口

```cpp
// src/windows/app/attributes.cpp
attributes.with_acrylic()            // 换成亚克力
          .with_always_on_top(true)  // 置顶
          .with_tool_window(true)    // 不进任务栏与 Alt+Tab
          .with_corner(platform::CornerPreference::RoundSmall);
```

### 3. 二级窗口关闭行为

```cpp
// src/main.cpp
windows::custom_terminal::open(handle, windows::CloseBehavior::Destroy); // 关闭即销毁（默认）
windows::custom_terminal::open(handle, windows::CloseBehavior::Hide);    // 关闭仅隐藏，可复用
```

### 4. 逃生通道

```cpp
platform::WindowAttributes()
    // 直接注入任意 DWM 属性
    .with_raw_attribute<uint32_t>(1029 /* DWMWA_MICA_EFFECT */, 1u)
    // 拿到裸 HWND 做任意 Win32 调用
    .with_custom_action([](HWND hwnd) { ::SetWindowTextW(hwnd, L"Hello"); });
```

## ⚠️ 已知注意事项

1. **NVIDIA + OpenGL 后端**：透明区域可能渲染成黑色，请在 NVIDIA 控制面板把
   `OpenGL GDI 兼容性` 改为「优先兼容性」，或改用软件 / 其它渲染后端。
2. **原生按钮终端**：DWM 边框配置在窗口可见前完成（CBT Hook 中），若 Hook 安装失败会自动
   退化为事件循环中的补丁安装（可能出现一帧边框闪动）。
3. **本移植版与 Rust 版的差异**：`DwmDefWindowProc` 在原生终端里对 `WM_NCCALCSIZE`
   的返回保持与 Rust 版相同的处理顺序；标题栏拖拽使用 Win32 原生拖拽而非 winit API。

## 📄 License

与上游一致，MIT。
