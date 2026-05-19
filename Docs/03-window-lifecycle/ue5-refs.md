# UE5 源码对照 — 步 03

UE5 的窗口与事件系统不用 GLFW，自己实现一套基于 Win32 的：

## 窗口对象

- `Engine/Source/Runtime/ApplicationCore/Public/Windows/WindowsWindow.h`
- `Engine/Source/Runtime/ApplicationCore/Private/Windows/WindowsWindow.cpp`
  - `FWindowsWindow` 继承 `FGenericWindow`
  - `Initialize()` 内部调用 `CreateWindowEx`（含 `WS_OVERLAPPEDWINDOW` 等 style）
  - 提供 `Show` / `Hide` / `Minimize` / `Maximize` / `Restore` 等抽象
  - 类比 GLFW：`glfwCreateWindow` ≈ `FWindowsWindow::Initialize`

## 消息处理

- `Engine/Source/Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp`
  - `FWindowsApplication::AppWndProc` 是注册到 `RegisterClassEx` 的回调
  - `ProcessMessage` 内部 `switch(Msg)` 处理 `WM_KEYDOWN` / `WM_CLOSE` / `WM_SIZE` / `WM_PAINT` 等
  - 类比 GLFW：`glfwSetKeyCallback` 对应到 `WM_KEYDOWN` case 分支

## 事件分发

- `Engine/Source/Runtime/ApplicationCore/Public/GenericPlatform/GenericApplicationMessageHandler.h`
  - `FGenericApplicationMessageHandler` 是消息分发接口
  - UE Slate UI 系统实现这个接口，把 Win32 消息转成 Slate 事件
  - 类比 GLFW：GLFW 直接调用注册的 callback；UE 多一层 dispatcher

## 主循环

- `Engine/Source/Runtime/Launch/Private/LaunchEngineLoop.cpp`
  - `FEngineLoop::Tick` 每帧调 `FPlatformApplicationMisc::PumpMessages`
  - 内部走 `PeekMessage` + `TranslateMessage` + `DispatchMessage` 三件套
  - 类比 GLFW：`while (!glfwWindowShouldClose) glfwPollEvents()` 对应到 `FEngineLoop::Tick` 主循环

## 关闭与销毁

- `Engine/Source/Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp`（`WM_CLOSE` handler）
  - UE 处理 `WM_CLOSE` 时调 `MessageHandler->OnWindowClose(Window)`，最终触发 `RequestEngineExit`
  - 类比 GLFW：`glfwSetWindowShouldClose` 是 GLFW 抽象的等价物

## 哲学差异

| 点 | GLFW（我们用的） | UE5 |
|---|---|---|
| 抽象层数 | 0—1（直接 callback） | 3—4（WndProc → MessageHandler → Slate → Game） |
| 多线程 | 单线程 | 多线程（game / render / RHI / RHI translate） |
| 用户写主循环 | 自己写 while | UE `FEngineLoop` 托管，你写 Tick 函数 |
| 输入 | callback 即原始数据 | 经过 Slate / EnhancedInput / Player Controller 多层抽象 |

WNanite 选 GLFW：**直接 = 易看清**。学清楚 GLFW 那套，再回头看 UE 那套就更容易了。

**注意**：本仓库不复制 UE5 源码。所有 UE5 文件路径仅作为对照参考（License）。
