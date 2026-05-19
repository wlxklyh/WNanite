# 步 02 — 引入 GLFW（FetchContent）设计文档

**日期**：2026-05-20
**作者**：linyanhou98@gmail.com + Claude
**状态**：草案（待用户审阅）
**附属于**：`Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md` §5 Phase A 步 02

---

## 1. 目标

通过 CMake `FetchContent` 拉 GLFW 3.4 源码，关掉它的 docs / examples / tests / install，静态链接到 `wnanite` target；在 `main.cpp` 里调用 `glfwInit` / 打印 GLFW 版本字符串 / `glfwTerminate`，证明库真的链上来且**运行时可调**。

**显式不做：** 创建窗口、监听事件、引入 GLFW 输入回调。窗口生命周期是步 03 的事。

## 2. 已锁定的技术决策

| 维度 | 决定 | 理由 |
|---|---|---|
| 引入方式 | CMake `FetchContent` | 与 master spec §3 一致；统一所有第三方依赖入口；后续 DXC / cgltf / D3D12MA 同模式 |
| GLFW 版本 pin | release tag `3.4`（2024-02 发布） | 最新稳定 release；可复现 |
| 链接方式 | 静态库（GLFW 默认） | 学习项目避免 DLL 找路径的坑 |
| 链接可见性 | `PRIVATE` | `wnanite` 是 leaf executable，无传递依赖 |
| GLFW sub-options | 关掉 docs / examples / tests / install | 减少 ~15 秒首次配置时间，不引无关 target |
| FetchContent 位置 | 根 `CMakeLists.txt`（与现有代码并列） | YAGNI；4-5 个依赖后再拆 `cmake/Dependencies.cmake` |
| 验收信号 | stdout 多出一行 `GLFW <version>` | 不仅链接通过，运行时也能调到 |

## 3. 文件改动清单

| 文件 | 动作 | 关键变化 |
|------|------|---------|
| `CMakeLists.txt` | 修改 | 在 `add_executable` 前加 FetchContent 块（约 15 行）；之后加 `target_link_libraries` |
| `src/main.cpp` | 修改 | 添加 `<GLFW/glfw3.h>` include 与 init/version-print/terminate 调用 |
| `Docs/02-glfw-bringup/` | 新建目录 | `README.md`（七节）、`ue5-refs.md`、`screenshots/.gitkeep` |
| `CLAUDE.md` | 修改 | §10 Phase A 步 02 由 `[ ]` 改 `[x]` 并附 Docs 链接 |
| `.gitignore` | 无变 | FetchContent 缓存默认在 `build-debug/_deps/`，已被 `build-*/` 规则排除 |

## 4. CMakeLists.txt 关键 diff

在现有 `add_executable(wnanite src/main.cpp)` **之前** 插入：

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
```

在 `add_executable` 之后追加：

```cmake
# === 链接第三方 ===
target_link_libraries(wnanite PRIVATE glfw)
```

## 5. src/main.cpp 完整新版本

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

## 6. 预期 stdout

```
hello, WNanite!
GLFW 3.4.0 Win32 WGL Null EGL OSMesa VisualC
```

（第二行的具体 backend 字符串顺序可能因平台 build 选项略变；只要以 `GLFW 3.4.0` 起头即视作通过。）

## 7. 验证

- **编译**：Debug + Release 两 preset 均通过；`src/main.cpp` 在 `/W4 /permissive-` 下零警告；GLFW 头文件因 `FetchContent_Declare(... SYSTEM)` 不参与警告统计
- **运行**：Debug 与 Release 二进制 stdout 各包含两行，退出码 0
- **回归**：原步 01 验收信号（`hello, WNanite!` 仍存在）继续通过

不写单元测试（CLAUDE.md §7：DX12/GLFW API 层不写单测）。无 golden image（无可见输出）。无 ai-learn HTML（依赖管理类，按 §5.3 跳过）。

## 8. 风险与缓解

| 风险 | 影响 | 缓解 |
|---|---|---|
| 首次 configure 拉源码 ~3 MB + 编译 ~20-40s | 配置时间变长 | `GIT_SHALLOW TRUE` 减少克隆体积；只首次慢，后续 CMake 缓存命中 |
| 离线环境 configure 失败 | 无法初始化项目 | 学习项目要求联网；CI 阶段可缓存 `_deps/` |
| ~~`/W4` 下 GLFW 头文件警告~~ | ~~阻碍未来 `-W -Werror`~~ | 已通过 `SYSTEM` 标记预先解决 |
| `glfwInit()` 在某些隔离环境返回 false | 验收失败 | 步 02 仅本地手跑；CI 上线时（步 ≥ 36）再处理 |

## 9. 范围外（明确划清）

- 创建窗口 / `glfwCreateWindow` —— 步 03
- 处理事件循环 / `glfwPollEvents` —— 步 03
- 输入回调 —— 步 03 之后
- DPI / HiDPI 处理 —— 后置
- 其他 GLFW 模块（Vulkan loader hook、joystick、monitor 枚举）—— 不在 harness 范围

## 10. 下一步流程

1. 用户审阅本文档
2. 同意后调 `superpowers:writing-plans` 出步 02 实施计划
3. 实施 + 验证 + Docs + step-02 commit + push origin
