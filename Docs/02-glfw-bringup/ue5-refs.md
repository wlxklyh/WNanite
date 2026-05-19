# UE5 源码对照 — 步 02

UE5 不使用 GLFW，自己写跨平台 Application 抽象。最接近 GLFW 角色的对应实现：

## 跨平台抽象层（接口）

- `Engine/Source/Runtime/ApplicationCore/Public/GenericPlatform/GenericApplication.h`
  - 抽象基类 `FGenericApplication`，定义 `ProcessMessages`、`PumpMessages`、`PollGameDeviceState` 等接口
  - 类比 GLFW：`glfwPollEvents` 对应 `FGenericApplication::PumpMessages`

## Windows 实现

- `Engine/Source/Runtime/ApplicationCore/Public/Windows/WindowsApplication.h`
- `Engine/Source/Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp`
  - `FWindowsApplication` 继承 `FGenericApplication`
  - 内部直接调 Win32 `CreateWindowEx` / `RegisterClassEx` / `GetMessage` / `DispatchMessage`
  - 比 GLFW 重：包含 HiDPI、IME、Touch、Tablet、Accessibility

- `Engine/Source/Runtime/Core/Public/Windows/WindowsPlatformMisc.h`
  - 各类 Win32 平台工具（剪贴板、调试、信息查询）

## 初始化入口

- `Engine/Source/Runtime/Launch/Private/LaunchEngineLoop.cpp`
  - `FEngineLoop::PreInit` 中调用 `FPlatformApplicationMisc::CreateApplication()` 创建 `FWindowsApplication` 实例
  - 类比 GLFW：`glfwInit()` 对应 `CreateApplication()`

## 设计哲学差异

- **GLFW**：单一 process-wide 状态机（`glfwInit` 全局），用户自己跑 `while(!glfwWindowShouldClose)` 循环
- **UE5**：Application 是个对象，Engine 在 main loop 里替你 pump，业务层订阅事件

WNanite 选 GLFW：学习项目要短路径。UE 那一套抽象层级在我们这个规模下属于过度设计；等真要复刻 UE 风格的 Slate / Application 时（不会很快），可以回头看 `WindowsApplication.cpp`。

**注意**：本仓库不复制 UE5 源码。所有 UE5 文件路径仅作为对照参考（License）。
