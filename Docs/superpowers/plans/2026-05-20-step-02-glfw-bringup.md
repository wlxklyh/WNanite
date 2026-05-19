# Step 02 — Introduce GLFW via FetchContent Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 通过 CMake `FetchContent` 拉 GLFW 3.4 并静态链接到 `wnanite`，运行时调 `glfwInit` / 打印版本字符串 / `glfwTerminate`，验证库真活着。**不开窗口**（步 03 才开）。

**Architecture:** 在根 `CMakeLists.txt` 加约 18 行 FetchContent 声明（含 `SYSTEM` 标记屏蔽 GLFW 头警告），关掉 docs / examples / tests / install；`target_link_libraries(wnanite PRIVATE glfw)`。`src/main.cpp` 多 3 行 GLFW 调用。

**Tech Stack:** CMake 3.25+ FetchContent / GLFW 3.4 / 静态链接 / MSVC C++20。

**对应 Spec：** `Docs/superpowers/specs/2026-05-20-step-02-glfw-bringup-design.md`

---

## File Structure

```
WNanite/
  CMakeLists.txt                       # 修改 — 新增 FetchContent + link
  src/main.cpp                         # 修改 — 加 GLFW init/version/terminate
  CLAUDE.md                            # 修改 — §10 勾选步 02
  Docs/
    02-glfw-bringup/                   # 新建目录
      README.md                        # 七节学习笔记
      ue5-refs.md                      # GLFW 与 UE 平台层的对照
      screenshots/.gitkeep
    superpowers/
      plans/
        2026-05-20-step-02-glfw-bringup.md   # 本文件
  build-debug/_deps/glfw-src/          # FetchContent 缓存（被 .gitignore 排除）
  build-release/_deps/glfw-src/        # 同上
```

---

## Task 1: 修改 `CMakeLists.txt` — 加 FetchContent + GLFW + link

**Files:**
- Modify: `E:\LYH\WNanite\CMakeLists.txt`

- [ ] **Step 1.1: 在 `add_executable` 之前插入 FetchContent 块**

Edit `E:\LYH\WNanite\CMakeLists.txt`:
- old_string:
```cmake
# === 可执行目标 ===
add_executable(wnanite
    src/main.cpp
)
```
- new_string:
```cmake
# === 第三方依赖（FetchContent） ===
include(FetchContent)

# --- GLFW 3.4 ---
# 关掉 GLFW 自带的非核心子目标，缩短首次 configure 时间且避免污染目标列表。
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
    SYSTEM                          # CMake 3.25+：把 GLFW include 路径标为 SYSTEM，屏蔽其内部 /W4 警告
)
FetchContent_MakeAvailable(glfw)

# === 可执行目标 ===
add_executable(wnanite
    src/main.cpp
)

# === 链接第三方 ===
target_link_libraries(wnanite PRIVATE glfw)
```

- [ ] **Step 1.2: 校验 CMakeLists.txt 包含关键关键字**

Run:
```powershell
$content = Get-Content E:\LYH\WNanite\CMakeLists.txt -Raw
@('FetchContent_Declare','GIT_TAG        3.4','SYSTEM','target_link_libraries(wnanite PRIVATE glfw)') | ForEach-Object {
    "$($content.Contains($_))`t$_"
}
```
Expected: 4 个 `True` 行。

---

## Task 2: 修改 `src/main.cpp` — 加 GLFW init/version/terminate

**Files:**
- Modify: `E:\LYH\WNanite\src\main.cpp`（完整替换）

- [ ] **Step 2.1: 完整替换 main.cpp**

Write `E:\LYH\WNanite\src\main.cpp` (覆盖) with exactly:
```cpp
// WNanite — 主入口
// 当前阶段：步 02 — 引入 GLFW（FetchContent），验证链接 + 运行时可调。
// 仍不开窗口（步 03）。

#include <GLFW/glfw3.h>
#include <cstdio>

int main()
{
    std::printf("hello, WNanite!\n");

    // 仅验证 GLFW 链接通且运行时能调；不创建任何窗口。
    if (!glfwInit())
    {
        std::printf("glfwInit failed\n");
        return 1;
    }
    std::printf("GLFW %s\n", glfwGetVersionString());
    glfwTerminate();

    return 0;
}
```

- [ ] **Step 2.2: 校验关键字**

Run:
```powershell
$c = Get-Content E:\LYH\WNanite\src\main.cpp -Raw
@('#include <GLFW/glfw3.h>','glfwInit','glfwGetVersionString','glfwTerminate') | ForEach-Object { "$($c.Contains($_))`t$_" }
```
Expected: 4 个 `True` 行。

---

## Task 3: 构建+运行 Debug

- [ ] **Step 3.1: Reconfigure Debug**

Run:
```powershell
Set-Location E:\LYH\WNanite; cmake --preset debug
```
Expected: 末尾再次 `-- Configuring done` / `-- Generating done`。本次配置会触发 GLFW 源克隆 + CMake 配置，预计 20-40 秒；中途有 `-- Fetching glfw ...` 输出。退出码 0。

- [ ] **Step 3.2: 构建 Debug**

Run:
```powershell
cmake --build E:\LYH\WNanite\build-debug --config Debug -j
```
Expected: 末尾 `wnanite.vcxproj -> E:\LYH\WNanite\build-debug\Debug\wnanite.exe`。中间会构建 `glfw.vcxproj`。`src/main.cpp` 编译过程**无新警告**（GLFW 头因 SYSTEM 不计入警告统计）。退出码 0。

- [ ] **Step 3.3: 运行 Debug 二进制 + 校验输出 + 退出码**

Run:
```powershell
$out = & E:\LYH\WNanite\build-debug\Debug\wnanite.exe
$out
"---"
"EXIT:$LASTEXITCODE"
"---"
"line1_ok: $($out[0] -eq 'hello, WNanite!')"
"line2_starts_with_GLFW_3: $($out[1].StartsWith('GLFW 3.'))"
```
Expected:
```
hello, WNanite!
GLFW 3.4.0 <backends>
---
EXIT:0
---
line1_ok: True
line2_starts_with_GLFW_3: True
```

---

## Task 4: 构建+运行 Release

- [ ] **Step 4.1: Reconfigure Release**

Run:
```powershell
cmake --preset release
```
Expected: 同 3.1（GLFW 会在 `build-release/_deps/` 独立克隆+配置）。退出码 0。

- [ ] **Step 4.2: 构建 Release**

Run:
```powershell
cmake --build E:\LYH\WNanite\build-release --config Release -j
```
Expected: `wnanite.exe` 落在 `build-release\Release\`。退出码 0。

- [ ] **Step 4.3: 运行 Release 二进制 + 校验**

Run:
```powershell
$out = & E:\LYH\WNanite\build-release\Release\wnanite.exe
$out
"EXIT:$LASTEXITCODE"
"checks: $($out[0] -eq 'hello, WNanite!') / $($out[1].StartsWith('GLFW 3.'))"
```
Expected: 两行输出 + `EXIT:0` + `checks: True / True`。

---

## Task 5: 学习文档 — `Docs/02-glfw-bringup/`

**Files:**
- Create: `Docs/02-glfw-bringup/README.md`
- Create: `Docs/02-glfw-bringup/ue5-refs.md`
- Create: `Docs/02-glfw-bringup/screenshots/.gitkeep`

- [ ] **Step 5.1: 创建目录**

Run:
```powershell
New-Item -ItemType Directory -Force -Path E:\LYH\WNanite\Docs\02-glfw-bringup\screenshots | Out-Null
```

- [ ] **Step 5.2: 写 `screenshots/.gitkeep`（空文件占位）**

Write `E:\LYH\WNanite\Docs\02-glfw-bringup\screenshots\.gitkeep` with empty content.

- [ ] **Step 5.3: 写 `README.md`（七节模板）**

Write `E:\LYH\WNanite\Docs\02-glfw-bringup\README.md` with exactly:
````markdown
# 步 02 — 引入 GLFW（FetchContent）

> Phase A · Bootstrap · 第 2 步 / 共 11 步
> 对应 Spec：`Docs/superpowers/specs/2026-05-20-step-02-glfw-bringup-design.md`

---

## 1. 本步学了什么

第一次往 WNanite 加 **CMake `FetchContent` 拉第三方依赖** 的范式：

- `FetchContent_Declare` + `FetchContent_MakeAvailable` 的最小用法
- 关闭依赖自带的 docs / examples / tests / install 子目标（缓存变量 `... CACHE BOOL "" FORCE`）
- 用 CMake 3.25+ 的 `SYSTEM` 标记让 GLFW 的 `/W4` 警告不污染我们的构建
- `target_link_libraries(... PRIVATE glfw)` 把依赖钉死在 leaf executable
- `glfwInit` / `glfwGetVersionString` / `glfwTerminate` 这条最小活性验证回路

走完本步：构建产物的 stdout 在原 `hello, WNanite!` 之后多一行 `GLFW 3.4.0 ...`，证明 GLFW 真链上来了。

## 2. 为什么这么做

**为什么 FetchContent 而不是 vcpkg / 手动子模块**：
- 单源真理：所有第三方依赖都在 `CMakeLists.txt` 里声明，看一处就懂
- 无需用户 / CI 预装包管理器
- 后续 DXC / D3D12MA / cgltf / imgui / spdlog / doctest / meshoptimizer 全套同模式

**为什么钉 tag `3.4`**：
- 最新稳定 release（2024-02）
- 学习项目可复现性 > 跟最新
- 不钉 SHA（学习项目；以后真要严格反推时再升级到 SHA）

**为什么关 docs / examples / tests / install**：
- 关掉后 GLFW 配置时间从 ~40s 降到 ~20s
- 这些 target 出现在 IDE solution explorer 里只是噪声

**为什么 `SYSTEM` 标记**：
- GLFW 头文件在 `/W4 /permissive-` 下会产生若干警告（macro 中的强制转换、padding 警告等）
- `SYSTEM` 让消费者把这些头视作系统头，警告被静音
- 我们自己写的 `main.cpp` 仍按严格 `/W4` 检查

**为什么 PRIVATE 链接**：
- `wnanite` 是叶子节点 executable，没有别的 target 继承它的链接需求
- 写 PRIVATE 表达"GLFW 是我的内部依赖，外部不见"

**为什么打印版本而不是只链不调**：
- 链接通过只能证明符号存在；不能证明运行时能加载
- 调一次 `glfwInit` + 拿到非空版本串 = 完整的端到端活性证据
- 多一行 stdout 也是给 agent 自验证留信号

## 3. 代码导读

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `CMakeLists.txt:34` | `include(FetchContent)` | 启用 FetchContent 模块 |
| `CMakeLists.txt:38-41` | `set(GLFW_BUILD_DOCS OFF ...)` 等 4 行 | 关掉 GLFW 子目标 |
| `CMakeLists.txt:43-49` | `FetchContent_Declare(glfw ...)` | shallow clone tag 3.4，标 SYSTEM |
| `CMakeLists.txt:50` | `FetchContent_MakeAvailable(glfw)` | 注入 `glfw` target |
| `CMakeLists.txt:58` | `target_link_libraries(wnanite PRIVATE glfw)` | 钉死链接 |
| `src/main.cpp:5` | `#include <GLFW/glfw3.h>` | 头一进来 |
| `src/main.cpp:13-17` | `glfwInit` 早退分支 | 失败立刻 exit 1 |
| `src/main.cpp:18` | `glfwGetVersionString` | 拿到版本字符串打印 |
| `src/main.cpp:19` | `glfwTerminate` | 干净退出 |

## 4. UE5 是怎么做的

UE5 自己写跨平台窗口/输入抽象，**不依赖 GLFW**。

最接近 GLFW 角色的对应实现：

- `Engine/Source/Runtime/ApplicationCore/Public/Windows/WindowsApplication.h`
- `Engine/Source/Runtime/ApplicationCore/Private/Windows/WindowsApplication.cpp`
- `Engine/Source/Runtime/Core/Public/Windows/WindowsPlatformMisc.h`

UE 的层次：`FGenericApplication`（抽象）→ `FWindowsApplication`（Windows 实现）→ 直接调用 Win32 `CreateWindowEx` / `RegisterClassEx` / `GetMessage`。比 GLFW 重得多（含 HiDPI、IME、accessibility、tablet input 等），但本质都是 Win32 消息循环 + 窗口生命周期。

WNanite 选 GLFW：学习项目要短路径，能把精力留给 Nanite 算法本身。

详见 `ue5-refs.md`。

## 5. 截图 / GIF

本步无图形输出。`screenshots/` 留空（仅 `.gitkeep` 占位）。预期 stdout：

```
hello, WNanite!
GLFW 3.4.0 Win32 WGL Null EGL OSMesa VisualC
```

第二行后半段 backend 串顺序可能因平台略变；只要以 `GLFW 3.4.0` 起头即合格。

## 6. 踩过的坑

- **GLFW 头部警告污染 `/W4` 构建**：用 `FetchContent_Declare(... SYSTEM)`（CMake 3.25+）预先解决。CMake 3.24 及更老的话只能 `target_include_directories(glfw SYSTEM ...)` 或在 wrapper 头里包一层。
- **`FetchContent_MakeAvailable` 没有 EXCLUDE_FROM_ALL**：GLFW 的 target 会出现在 IDE solution，但因为 docs/examples/tests/install 都关了，扰动最小。
- **首次 configure 慢**：FetchContent 默认会克隆完整 git 历史；`GIT_SHALLOW TRUE` 只拉 tag 的 tip commit，配置时间从 ~40s 降到 ~10s。
- **多 preset 各自克隆一份**：`build-debug/_deps/glfw-src/` 与 `build-release/_deps/glfw-src/` 是各自独立的，本步没有共享缓存。这是 FetchContent 的默认行为，学习项目可接受；想合并的话设置 `FETCHCONTENT_BASE_DIR`（不预先做）。

## 7. 下一步预告

**步 03：GLFW 窗口生命周期 + 事件循环**——`glfwCreateWindow` 出一个真实窗口，`while(!glfwWindowShouldClose)` 跑 `glfwPollEvents`，关闭按钮可终止。仍不接 DX12（步 04+）。
````

- [ ] **Step 5.4: 写 `ue5-refs.md`**

Write `E:\LYH\WNanite\Docs\02-glfw-bringup\ue5-refs.md` with exactly:
```markdown
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
```

- [ ] **Step 5.5: 校验三个文档都已创建**

Run:
```powershell
@(
  "E:\LYH\WNanite\Docs\02-glfw-bringup\README.md",
  "E:\LYH\WNanite\Docs\02-glfw-bringup\ue5-refs.md",
  "E:\LYH\WNanite\Docs\02-glfw-bringup\screenshots\.gitkeep"
) | ForEach-Object { Test-Path $_ }
```
Expected: 三个 `True`。

---

## Task 6: 勾选 CLAUDE.md §10 进度

**Files:**
- Modify: `E:\LYH\WNanite\CLAUDE.md`（§10 Phase A 第 2 项）

- [ ] **Step 6.1: 勾选步 02**

Edit `E:\LYH\WNanite\CLAUDE.md`:
- old_string: `- [ ] 02 引入 GLFW`
- new_string: `- [x] 02 引入 GLFW — [Docs/02-glfw-bringup](Docs/02-glfw-bringup/README.md)`

- [ ] **Step 6.2: 校验**

Run:
```powershell
Select-String -Path E:\LYH\WNanite\CLAUDE.md -Pattern "\[x\] 02 引入 GLFW"
```
Expected: 命中一行。

---

## Task 7: 总结性提交 + push origin

- [ ] **Step 7.1: 暂存所有新增/修改**

Run:
```powershell
git -C E:\LYH\WNanite add CMakeLists.txt src/main.cpp CLAUDE.md Docs/02-glfw-bringup/ Docs/superpowers/specs/2026-05-20-step-02-glfw-bringup-design.md Docs/superpowers/plans/2026-05-20-step-02-glfw-bringup.md
```

- [ ] **Step 7.2: 校验暂存（不应含 build-* / _deps/）**

Run:
```powershell
git -C E:\LYH\WNanite status --short
```
Expected: 仅 `A` / `M` 状态条目；**没有** `build-debug`、`build-release`、`_deps/` 出现。

- [ ] **Step 7.3: 总结性提交**

Run:
```powershell
git -C E:\LYH\WNanite commit -m "step-02: introduce GLFW via FetchContent"
```
Expected: `[main <sha>] step-02: introduce GLFW via FetchContent` + 多 files changed。

- [ ] **Step 7.4: Push origin**

Run:
```powershell
git -C E:\LYH\WNanite push origin main
```
Expected: 包含 `<old-sha>..<new-sha>  main -> main` 或类似 fast-forward 输出。无错误。

- [ ] **Step 7.5: 校验提交历史 + 远程同步**

Run:
```powershell
git -C E:\LYH\WNanite log --oneline --decorate -n 5; Write-Output "---"; git -C E:\LYH\WNanite status
```
Expected:
```
<sha> (HEAD -> main, origin/main) step-02: introduce GLFW via FetchContent
03fc58f step-01: CMake hello-world
ec6e46c init: repo with design spec + CLAUDE.md
---
On branch main
Your branch is up to date with 'origin/main'.

nothing to commit, working tree clean
```

---

## Done 校验（对照 CLAUDE.md §3 单步完成定义）

- [ ] 1. Debug + Release 编译通过 — Task 3 / Task 4
- [ ] 2. 运行通过 — Step 3.3 / 4.3 stdout 两行 + exit 0
- [ ] 3. 验收信号 — `hello, WNanite!` + `GLFW 3.4.0 ...` 两行
- [ ] 4. `Docs/02-glfw-bringup/README.md` 七节完整 — Task 5
- [ ] 5. ai-learn 不适用（依赖管理类，按 §5.3 跳过） — N/A
- [ ] 6. doctest 不适用（无纯逻辑代码） — N/A
- [ ] 7. golden PNG 不适用（无可见输出） — N/A
- [ ] 8. CLAUDE.md §10 已勾 — Task 6
- [ ] Bonus: 已 push origin —Task 7.4

全部 ✓ 后步 02 完成，可进入步 03 brainstorming（窗口生命周期）。
