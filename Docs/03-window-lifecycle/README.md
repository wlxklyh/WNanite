# 步 03 — GLFW 窗口生命周期 + 事件循环

> Phase A · Bootstrap · 第 3 步 / 共 11 步
> 对应 Spec：`Docs/superpowers/specs/2026-05-20-step-03-window-lifecycle-design.md`

---

## 1. 本步学了什么

WNanite 第一次出**真正的窗口**：

- `glfwSetErrorCallback` —— 在 `glfwInit` 之前就把错误 channel 立起来
- `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)` —— **关键**：禁止 GLFW 默认创建 OpenGL 上下文（后面 DX12 要拿干净的 HWND）
- `glfwCreateWindow` —— 创窗口主体
- `glfwSetKeyCallback` —— 注册键盘回调（ESC 关窗）
- `while (!glfwWindowShouldClose(window)) glfwPollEvents();` —— 事件循环
- `glfwDestroyWindow` + `glfwTerminate` —— 清理顺序

走完本步：跑 `wnanite.exe` 弹出 1280×720 窗口（标题 `WNanite — step 03`），可关闭、可 ESC 退出。**仍不接 DX12**——窗口内容由 OS 决定。

## 2. 为什么这么做

**为什么 `GLFW_CLIENT_API = GLFW_NO_API`**：

GLFW 默认行为是给每个窗口创建 OpenGL 上下文。我们将来要用 DX12，这意味着：
- OpenGL 上下文会占用窗口的 device context
- DX12 通过 HWND 创建 swap chain 时可能与 GL 上下文冲突
- 即使不冲突，多一份未用的上下文是浪费 + 调试噪声

`GLFW_NO_API` 明确告诉 GLFW："我会自己处理图形 API，你只负责窗口"。

**为什么错误回调注册在 `glfwInit` 之前**：

`glfwInit` 自己也可能产生错误（找不到 X server、Win32 API 调用失败等）。如果在 init 之后才设回调，init 失败的错误就 silent 丢了。提前注册 = 所有错误都能看到。

**为什么把 callback 放在匿名 namespace**：

C 风格回调（函数指针签名）必须用 `extern "C"` 友好的自由函数，不能是 lambda（不带捕获也算）。放在匿名 namespace 里：
- 防止与其他 translation unit 同名函数 ODR 冲突
- 表达"内部链接"语义

**为什么 ESC 退出走 key callback 而不是主循环里轮询**：

GLFW 主循环里用 `glfwGetKey(window, GLFW_KEY_ESCAPE)` 轮询也行，但 callback 是事件驱动——按下的瞬间就响应，没有"按了一帧没被 poll 到"的边界条件。后续相机输入（步 31）会同时用 callback（离散事件）+ 轮询（连续状态），本步先建 callback 这条路径。

**为什么 destroy 在 terminate 之前**：

GLFW 的资源拥有关系：`Window` 属于 GLFW 全局状态。`glfwTerminate` 会自动销毁所有窗口，但显式 destroy 让生命周期清晰，也方便后续多窗口场景。

**为什么主循环不 sleep**：

`glfwPollEvents` 立即返回，CPU 单核会接近 100%。步 03 不解决——步 08 引入 swap chain `Present` 自带垂直同步等待。学习项目里"明知问题但暂不解决"也是态度，比"提前优化但不知道为什么"健康。

## 3. 代码导读

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `src/main.cpp:12-15` | `on_glfw_error` | error callback：写 stderr |
| `src/main.cpp:18-25` | `on_glfw_key` | key callback：ESC → set should-close |
| `src/main.cpp:33` | `glfwSetErrorCallback(on_glfw_error)` | 必须在 `glfwInit` 之前 |
| `src/main.cpp:44` | `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)` | 禁止默认 GL 上下文，**最关键的一行** |
| `src/main.cpp:46-48` | `glfwCreateWindow(1280, 720, ...)` | 创窗口主体 |
| `src/main.cpp:55` | `glfwSetKeyCallback(window, on_glfw_key)` | 必须在 create 之后（window 才存在） |
| `src/main.cpp:59-62` | `while (!glfwWindowShouldClose...)` | 事件循环 |
| `src/main.cpp:64-65` | `glfwDestroyWindow` / `glfwTerminate` | 反向销毁 |

## 4. UE5 是怎么做的

UE5 不用 GLFW，自己写 `FWindowsWindow`：

- `Engine/Source/Runtime/ApplicationCore/Public/Windows/WindowsWindow.h`
- `Engine/Source/Runtime/ApplicationCore/Private/Windows/WindowsWindow.cpp`

主要差异：

| 角色 | GLFW | UE5 |
|---|---|---|
| 创建窗口 | `glfwCreateWindow` | `FWindowsWindow::Initialize` 内调 `CreateWindowEx` |
| 消息泵 | `glfwPollEvents` | `FWindowsApplication::PumpMessages` 调 `PeekMessage` + `DispatchMessage` |
| 消息处理 | GLFW 内部 WndProc | `FWindowsApplication::ProcessMessage` switch on `Msg` |
| ESC 退出 | 用户自己写 key callback | UE Slate 系统转发到 `FCommonInputModule` |
| 全局状态 | `glfwInit` 一次 | `FPlatformApplicationMisc::CreateApplication` 单次 |

UE 的事件循环不是简单 while，而是和 game thread / render thread / RHI thread 协调。学习项目下我们这套单线程 GLFW 循环更易看清窗口本身的生命周期。

详见 `ue5-refs.md`。

## 5. 截图 / GIF

预期：1280×720 窗口，标题栏 `WNanite — step 03`，窗口主体内容由 OS 决定（黑 / 显示器残影 / 桌面碎片都正常）。

GIF：关闭按钮按下时窗口消失。

（实际截图待人工抓 + 落到 `screenshots/` 后补）

## 6. 踩过的坑

- **忘记 `GLFW_NO_API` hint**：默认会创 OpenGL 上下文。本步暂时看不出影响，但步 04 DX12 接入时容易翻车——HWND 已绑 GL 上下文。**这一行是步 03 最值钱的细节**。
- **`glfwSetErrorCallback` 注册晚了**：如果放在 `glfwInit` 之后，init 自己的错误就 silent 丢失。规则：error callback 永远第一个注册。
- **lambda 不能直接做 GLFW 回调**：即使是无捕获 lambda，编译能过但容易引入 ABI 不一致问题。**自由函数**（匿名 namespace 包一下）是干净选择。
- **`/W4` 下 callback 未用参数报 C4100**：参数 anonymize（`int /*scancode*/`）是消警告的 idiom，比 `(void)scancode;` 更干净。
- **ESC 触发后窗口不会立刻消失**：`glfwSetWindowShouldClose(window, GLFW_TRUE)` 只是设标志位，下一次循环迭代检查时才退出 while。理解这一点对后续渲染循环很重要。
- **CPU 占用高**：单核 ~100%。预期行为，步 08 解决。
- **PowerShell em dash 编码坑**：用 `Get-Content -Raw | .Contains("WNanite — step 03")` 校验文件内容时，命令行字符串的 em dash 可能被 PS 重编码导致 false negative。校验改用 `Select-String` 或读文件后单独 grep 更稳。

## 7. 下一步预告

**步 04：DXGI Factory + Adapter 枚举 + 日志**——第一次接 DX12！创建 `IDXGIFactory6`，枚举系统里所有显卡，把每张的描述（厂商、显存、Feature Level）打印出来。**还不创 D3D12 device**，纯 DXGI 探测。
