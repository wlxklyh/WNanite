# 步 03 — GLFW 窗口生命周期 + 事件循环 设计文档

**日期**：2026-05-20
**作者**：linyanhou98@gmail.com + Claude
**状态**：草案（待用户审阅）
**附属于**：`Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md` §5 Phase A 步 03

---

## 1. 目标

让 WNanite 第一次出**真正的窗口**：1280×720 / 可关闭 / 可按 ESC 退出 / 错误能通过 callback 看到。仍**不接 DX12**——窗口背景由 OS 决定（黑色或残影），不调任何渲染 API。

**关键中位决定**：提前用 `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)`，避免 GLFW 默认创建 OpenGL 上下文导致后续 DX12 抢 HWND 时冲突。

## 2. 已锁定的技术决策

| 维度 | 决定 | 理由 |
|---|---|---|
| 窗口尺寸 | 1280 × 720（HD） | 渲染 demo 标准默认；后续 imgui 也好布局 |
| 窗口标题 | `WNanite — step 03` | 每步可改值作进度指示 |
| Resizable | yes（GLFW 默认） | 提前用默认行为暴露；步 ≥ 11 处理 swap chain resize |
| `GLFW_CLIENT_API` | `GLFW_NO_API` | **必须**——避免 GLFW 默认创 OpenGL 上下文与 DX12 抢资源 |
| 错误处理 | `glfwSetErrorCallback` → 写 stderr | 学习项目首步把错误 channel 立起来 |
| 退出条件 | 关闭按钮 ∨ ESC 键 | demo 常规；省去开任务管理器 |
| 主循环节奏 | 纯 `glfwPollEvents()` 不 sleep | 步 03 不解决 CPU 占用；步 08 swap chain present 自带限速 |
| 代码组织 | 内联在 `main.cpp` | YAGNI；等步 ≥ 11 多帧管理时再抽 `Window` 类 |
| Console | 保留 | spdlog（步 12）之前所有输出都靠 stdout/stderr |

## 3. 文件改动清单

| 文件 | 动作 | 关键变化 |
|------|------|---------|
| `src/main.cpp` | 修改 | 加 error callback / key callback；调用 `glfwCreateWindow` + 主循环 + 销毁 |
| `Docs/03-window-lifecycle/` | 新建目录 | `README.md`（七节）、`ue5-refs.md`、`screenshots/.gitkeep` |
| `CLAUDE.md` | 修改 | §10 Phase A 步 03 由 `[ ]` 改 `[x]` 并附 Docs 链接 |
| `CMakeLists.txt` | 无变 | GLFW 已链好，本步不动 CMake |

## 4. src/main.cpp 完整新版本

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

## 5. 预期行为

启动后：
1. stdout 仍有两行 `hello, WNanite!` / `GLFW 3.4.0 ...`（继承自步 02）
2. 弹出一个 1280×720 标题 `WNanite — step 03` 的窗口
3. 窗口内容由 OS 决定（黑色 / 显示器残留 / WS_BORDER 框）——**GLFW 不做清屏**
4. 点关闭按钮 ✕ → 窗口消失，进程退出码 0
5. 或按 ESC → 同上
6. 故意把宽度传 0（手动改代码测） → stderr 输出 `GLFW error <code>: <desc>`，无 crash

## 6. 验证

- **编译**：Debug + Release 两 preset 均通过；`main.cpp` 在 `/W4 /permissive-` 下零警告（已用 `int /*scancode*/` 等屏蔽未用参数警告）
- **运行**：人工启动 → 窗口可见 → 关闭按钮或 ESC 退出 → 退出码 0
- **回归**：步 02 stdout 两行仍输出
- 不写单测（DX12/GLFW API 层，按 CLAUDE.md §7）
- 不录 golden image（窗口由 OS 渲，截不到稳定内容）
- 不调 ai-learn（API 流程类，按 CLAUDE.md §5.3）

## 7. 风险

| 风险 | 缓解 |
|---|---|
| CPU 单核 100% | 预期行为；步 08 引入 present 自然限速 |
| Resize 时调主循环无法响应 | GLFW 在某些平台 resize 会阻塞 `glfwPollEvents`；步 ≥ 11 加上 swap chain resize 时处理 |
| 没 `glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API)` 会怎样 | 默认创 OpenGL 上下文；步 04 用 DX12 拿 HWND 时上下文已被占用，DX12 device 创建可能 silent 失败或行为异常 |
| `glfwSetErrorCallback` 没设 | 错误 silent 丢弃，调试地狱。提前装好回调即解决 |

## 8. 范围外（明确划清）

- 任何 DX12 调用 / device / swap chain / 清屏 —— 步 04+
- 窗口图标 / 多窗口 / 多显示器枚举
- DPI / HiDPI 缩放（步 ≥ 11 swap chain 时考虑）
- 输入抽象（kb/mouse 状态机、键位重绑定）—— 步 31 相机时引入
- 帧率限制 / vsync —— 步 08 swap chain 自带

## 9. 下一步流程

1. 用户审阅本文档
2. 同意后调 `superpowers:writing-plans` 出步 03 实施计划
3. 实施 + 验证 + Docs + step-03 commit + push origin
