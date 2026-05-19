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
- **GLFW 触发 C 编译器探测 + pthread 测试**：GLFW 内部用 C 写，所以 CMake 会探测 MSVC 的 C 编译器并跑 `pthread` 探测。Windows 上 pthread 自然找不到（Win32 thread API 替代），属正常输出。

## 7. 下一步预告

**步 03：GLFW 窗口生命周期 + 事件循环**——`glfwCreateWindow` 出一个真实窗口，`while(!glfwWindowShouldClose)` 跑 `glfwPollEvents`，关闭按钮可终止。仍不接 DX12（步 04+）。
