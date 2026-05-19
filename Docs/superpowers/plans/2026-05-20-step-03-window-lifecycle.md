# Step 03 — GLFW Window Lifecycle Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 WNanite 第一次出真正的窗口（1280×720 / `WNanite — step 03` / 关闭按钮 + ESC 可退）。仍不接 DX12——窗口背景由 OS 决定，不调任何渲染 API。

**Architecture:** 全部内联在 `src/main.cpp`：匿名 `namespace` 包两个 callback（error → stderr，key → ESC 关窗），主流程 init → set hint NO_API → create window → set key callback → poll loop → destroy → terminate。

**Tech Stack:** GLFW 3.4（已链）/ MSVC C++20。

**对应 Spec：** `Docs/superpowers/specs/2026-05-20-step-03-window-lifecycle-design.md`

---

## File Structure

```
WNanite/
  src/main.cpp                           # 修改 — 加 callbacks + window 主循环
  CLAUDE.md                              # 修改 — §10 勾选步 03
  Docs/
    03-window-lifecycle/                 # 新建目录
      README.md                          # 七节学习笔记
      ue5-refs.md                        # 对照 UE5 WindowsWindow 实现
      screenshots/.gitkeep
    superpowers/
      plans/
        2026-05-20-step-03-window-lifecycle.md   # 本文件
```

---

## Task 1: 完整替换 `src/main.cpp`

**Files:**
- Modify: `E:\LYH\WNanite\src\main.cpp`（完整覆盖）

- [ ] **Step 1.1: 写新版 main.cpp**

Write `E:\LYH\WNanite\src\main.cpp` (覆盖) with exactly:
```cpp
// WNanite — 主入口
// 当前阶段：步 03 — GLFW 窗口生命周期 + 事件循环。
// 仍不接 DX12（步 04+）；窗口背景由 OS 决定，没有任何渲染。

#include <GLFW/glfw3.h>
#include <cstdio>

namespace
{
    // 把 GLFW 报告的错误都写到 stderr。
    // 后续步 12 spdlog 接入后，会把这里改成 spdlog::error。
    void on_glfw_error(int code, const char* description)
    {
        std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
    }

    // ESC 键 → 请求关闭窗口。
    void on_glfw_key(GLFWwindow* window, int key, int /*scancode*/,
                     int action, int /*mods*/)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
}

int main()
{
    std::printf("hello, WNanite!\n");

    // 错误回调要在 glfwInit 之前注册——init 自身的错误也能拿到。
    glfwSetErrorCallback(on_glfw_error);

    if (!glfwInit())
    {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    std::printf("GLFW %s\n", glfwGetVersionString());

    // 关键：禁止 GLFW 自动创 OpenGL/OpenGL ES 上下文。
    // 我们要把窗口 HWND 给 DX12（步 04+）。
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(1280, 720,
                                          "WNanite — step 03",
                                          nullptr, nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwSetKeyCallback(window, on_glfw_key);

    // 主循环：纯泵事件。
    // 暂不渲染、不限速；CPU 单核占用可能接近 100%，本步不解决。
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
```

- [ ] **Step 1.2: 校验关键字**

Run:
```powershell
$c = Get-Content E:\LYH\WNanite\src\main.cpp -Raw
@(
  'glfwSetErrorCallback',
  'glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)',
  'glfwCreateWindow(1280, 720',
  'WNanite — step 03',
  'glfwSetKeyCallback',
  'glfwSetWindowShouldClose',
  'while (!glfwWindowShouldClose(window))',
  'glfwDestroyWindow'
) | ForEach-Object { "$($c.Contains($_))`t$_" }
```
Expected: 8 个 `True` 行。

---

## Task 2: 构建 Debug + 构建 Release（编译验证）

- [ ] **Step 2.1: 构建 Debug**

Run（在 `E:\LYH\WNanite`）:
```powershell
cmake --build E:\LYH\WNanite\build-debug --config Debug -j
```
Expected: 末尾 `wnanite.vcxproj -> E:\LYH\WNanite\build-debug\Debug\wnanite.exe`，**无新警告**，退出码 0。

注意：`main.cpp` 在 `/W4 /permissive-` 下需零警告。`int /*scancode*/` / `int /*mods*/` 这种 anonymized 参数是消 C4100（unreferenced formal parameter）的标准 idiom。

- [ ] **Step 2.2: 构建 Release**

Run:
```powershell
cmake --build E:\LYH\WNanite\build-release --config Release -j
```
Expected: `wnanite.exe` 落在 `build-release\Release\`，退出码 0。

---

## Task 3: 交互验证（人工 — 关闭按钮 + ESC + 标题）

> 本步是 step 03 的核心验收：必须人眼看到窗口、必须人手按按钮/键。
> 步骤 20+ 引入 headless / 截图后，下面这些会被自动化。

- [ ] **Step 3.1: 启动 Debug 二进制（关闭按钮验证）**

Run:
```powershell
$p = Start-Process E:\LYH\WNanite\build-debug\Debug\wnanite.exe -PassThru
"PID: $($p.Id)"
"等待人工：1) 看到窗口（1280x720，标题 'WNanite — step 03'） 2) 点关闭按钮 ✕ 3) 等待退出"
$p.WaitForExit()
"ExitCode: $($p.ExitCode)"
```
Expected:
- 启动后弹出 1280×720 窗口，标题为 `WNanite — step 03`
- 窗口背景由 OS 决定（黑色 / 残影都正常；GLFW 不清屏）
- 鼠标点 ✕ → 窗口消失 → `WaitForExit` 返回 → `ExitCode: 0`

- [ ] **Step 3.2: 启动 Debug 二进制（ESC 验证）**

Run:
```powershell
$p = Start-Process E:\LYH\WNanite\build-debug\Debug\wnanite.exe -PassThru
"PID: $($p.Id)"
"等待人工：1) 看到窗口 2) 让窗口获得焦点 3) 按 ESC"
$p.WaitForExit()
"ExitCode: $($p.ExitCode)"
```
Expected: 按 ESC 后 → 窗口消失 → `ExitCode: 0`。

- [ ] **Step 3.3: 启动 Release 二进制（关闭按钮 + ESC 综合验证）**

Run:
```powershell
$p = Start-Process E:\LYH\WNanite\build-release\Release\wnanite.exe -PassThru
"PID: $($p.Id)"
"等待人工：测试两种退出方式之一"
$p.WaitForExit()
"ExitCode: $($p.ExitCode)"
```
Expected: 任一退出方式 → `ExitCode: 0`。

---

## Task 4: 学习文档 — `Docs/03-window-lifecycle/`

**Files:**
- Create: `Docs/03-window-lifecycle/README.md`
- Create: `Docs/03-window-lifecycle/ue5-refs.md`
- Create: `Docs/03-window-lifecycle/screenshots/.gitkeep`

- [ ] **Step 4.1: 创建目录**

Run:
```powershell
New-Item -ItemType Directory -Force -Path E:\LYH\WNanite\Docs\03-window-lifecycle\screenshots | Out-Null
```

- [ ] **Step 4.2: 写 `screenshots/.gitkeep`（空文件占位）**

Write `E:\LYH\WNanite\Docs\03-window-lifecycle\screenshots\.gitkeep` with empty content.

- [ ] **Step 4.3: 写 `README.md`（七节模板）**

Write `E:\LYH\WNanite\Docs\03-window-lifecycle\README.md` with exactly:
````markdown
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

## 7. 下一步预告

**步 04：DXGI Factory + Adapter 枚举 + 日志**——第一次接 DX12！创建 `IDXGIFactory6`，枚举系统里所有显卡，把每张的描述（厂商、显存、Feature Level）打印出来。**还不创 D3D12 device**，纯 DXGI 探测。
````

- [ ] **Step 4.4: 写 `ue5-refs.md`**

Write `E:\LYH\WNanite\Docs\03-window-lifecycle\ue5-refs.md` with exactly:
```markdown
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
```

- [ ] **Step 4.5: 校验三个文档都已创建**

Run:
```powershell
@(
  "E:\LYH\WNanite\Docs\03-window-lifecycle\README.md",
  "E:\LYH\WNanite\Docs\03-window-lifecycle\ue5-refs.md",
  "E:\LYH\WNanite\Docs\03-window-lifecycle\screenshots\.gitkeep"
) | ForEach-Object { Test-Path $_ }
```
Expected: 三个 `True`。

---

## Task 5: 勾选 CLAUDE.md §10 进度

**Files:**
- Modify: `E:\LYH\WNanite\CLAUDE.md`（§10 Phase A 第 3 项）

- [ ] **Step 5.1: 勾选步 03**

Edit `E:\LYH\WNanite\CLAUDE.md`:
- old_string: `- [ ] 03 GLFW 窗口生命周期`
- new_string: `- [x] 03 GLFW 窗口生命周期 — [Docs/03-window-lifecycle](Docs/03-window-lifecycle/README.md)`

- [ ] **Step 5.2: 校验**

Run:
```powershell
Select-String -Path E:\LYH\WNanite\CLAUDE.md -Pattern "\[x\] 03 GLFW 窗口生命周期"
```
Expected: 命中一行。

---

## Task 6: step-03 commit + push origin

- [ ] **Step 6.1: 暂存所有新增/修改**

Run:
```powershell
git -C E:\LYH\WNanite add src/main.cpp CLAUDE.md Docs/03-window-lifecycle/ Docs/superpowers/specs/2026-05-20-step-03-window-lifecycle-design.md Docs/superpowers/plans/2026-05-20-step-03-window-lifecycle.md
```

- [ ] **Step 6.2: 校验暂存（不应含 build-* / _deps/）**

Run:
```powershell
git -C E:\LYH\WNanite status --short
```
Expected: 仅 `A` / `M` 状态条目；**没有** `build-debug`、`build-release`、`_deps/` 出现。

- [ ] **Step 6.3: 总结性提交**

Run:
```powershell
git -C E:\LYH\WNanite commit -m "step-03: GLFW window lifecycle + event loop"
```
Expected: `[main <sha>] step-03: GLFW window lifecycle + event loop` + 多 files changed。

- [ ] **Step 6.4: Push origin**

Run:
```powershell
git -C E:\LYH\WNanite push origin main
```
Expected: `<old-sha>..<new-sha>  main -> main` 形式的 fast-forward。

- [ ] **Step 6.5: 校验**

Run:
```powershell
git -C E:\LYH\WNanite log --oneline --decorate -n 5; Write-Output "---"; git -C E:\LYH\WNanite status
```
Expected:
```
<sha> (HEAD -> main, origin/main) step-03: GLFW window lifecycle + event loop
34aa671 step-02: introduce GLFW via FetchContent
03fc58f step-01: CMake hello-world
ec6e46c init: repo with design spec + CLAUDE.md
---
On branch main
Your branch is up to date with 'origin/main'.

nothing to commit, working tree clean
```

---

## Done 校验（对照 CLAUDE.md §3 单步完成定义）

- [ ] 1. Debug + Release 编译通过 — Task 2
- [ ] 2. 运行通过 — Task 3 三次交互验证
- [ ] 3. 验收信号 — 窗口可见 / 关闭按钮+ESC 退出码 0 / stdout 两行
- [ ] 4. `Docs/03-window-lifecycle/README.md` 七节完整 — Task 4
- [ ] 5. ai-learn 不适用（API 流程类，按 §5.3 跳过） — N/A
- [ ] 6. doctest 不适用（无纯逻辑代码） — N/A
- [ ] 7. golden PNG 不适用（窗口由 OS 渲，无稳定可比） — N/A
- [ ] 8. CLAUDE.md §10 已勾 — Task 5
- [ ] Bonus: 已 push origin — Task 6.4

全部 ✓ 后步 03 完成，可进入步 04 brainstorming（DXGI Factory + Adapter 枚举）。
