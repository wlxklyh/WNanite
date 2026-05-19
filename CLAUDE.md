# WNanite — Claude 工作准则

本仓库的所有 Claude 会话必须遵循以下准则。
**优先级：本文件 > superpowers skills > 默认行为。**

---

## §0 项目目标

WNanite 是一个 **学习项目**：从零写 DX12 沙盒，复刻 UE5 Nanite 大部分流程，用于深入理解 DX12 与 Nanite 算法。

**优先级永远是：理解 > 速度 > 性能。**

- 当前阶段：**子项目 0 — Harness**（46 步）
- 设计文档：`Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md`
- 子项目地图（0→6）：见设计文档 §2
- 当前已完成步骤：见本文件 §10

**UE5 源码本地参考**：`D:\LYH\UE`（UE 5.6.1，分支 `W0+Main`）。完整 `D3D12RHI` / `Renderer/Private/Nanite` / `RHI` / `RenderCore` / `ApplicationCore` 源码在本地——这是本项目"复刻 UE5 Nanite"目标的权威参考。具体使用规则见 §1。

---

## §1 核心原则

**先调 skill，再动手。** 任何匹配 §2 触发表的任务，必须先用 `Skill` 工具调起对应 skill，按其流程执行——哪怕只有 1% 觉得相关也要调。不要凭记忆复述 skill 内容，每次都重新加载。

**证据先于断言。** 任何"完成 / 修好 / 通过"的说法之前，必须跑过验证命令并贴出输出。

**根因优先。** 遇到障碍不要用破坏性手段绕过（`--no-verify`、`reset --hard`、删 lock 文件等），先查根因。

**小步前进。** 1 步 = 1 主题，工作量 < 1 小时。详见 §3。

**遇事不决查 UE5。** DX12 / RHI / Nanite / RDG 任何"应该怎么写才地道"的不确定，优先到 `D:\LYH\UE\Engine\Source` 看 UE5 怎么实现。三条边界：
- **看**：用 Read 工具读 UE5 文件、引用 `file:line` 到 `Docs/NN-<topic>/ue5-refs.md`
- **不复制**：UE5 是 Source-Available License，不复制代码进本仓库；仅引用 + 用自己的话复述
- **不盲从**：UE5 路径包含大量历史包袱（兼容老硬件、多 RHI 后端、Slate/Game Thread 耦合等），学习项目可以简化。**理解 > 模仿**

---

## §2 强制 Skill 触发表

| 场景 | 必须调用的 skill |
|------|------------------|
| 新功能 / 新组件 / 新行为设计 | `superpowers:brainstorming` |
| 任何 bug / 测试失败 / 异常行为 | `superpowers:systematic-debugging` |
| 实现 feature 或 bugfix（涉及纯逻辑代码时） | `superpowers:test-driven-development` |
| 多步任务（≥3 步）动手前 | `superpowers:writing-plans` |
| 执行已有实施计划 | `superpowers:executing-plans` 或 `superpowers:subagent-driven-development` |
| 2 个以上独立无依赖任务 | `superpowers:dispatching-parallel-agents` |
| 需要工作区隔离的特性开发 | `superpowers:using-git-worktrees` |
| 声称"做完 / 修好 / 通过"之前 | `superpowers:verification-before-completion` |
| 任务完成 / 合并前 | `superpowers:requesting-code-review` |
| 收到 code review 反馈 | `superpowers:receiving-code-review` |
| 实现完成、准备合并 / 出 PR | `superpowers:finishing-a-development-branch` |
| **每步完成后写学习文档（算法/同步/数学类）** | **`ai-learn`** |
| **每步完成后写学习文档（DX12 API 流程类）** | 手写 `Docs/NN-<topic>/README.md`（不用 ai-learn） |
| 新建或修改 skill 本身 | `superpowers:writing-skills` |

---

## §3 学习步骤约定

每步必须满足：
- 工作量 < 1 小时
- 单一主题
- 编译通过 + 运行通过 + 可见或可测的产出
- 配套 `Docs/NN-<topic>/` 目录（结构见 §5）

**单步完成定义**（缺一不可）：
1. Debug + Release 编译通过
2. headless 或交互运行通过
3. 有验收信号（截图 / 日志 / 测试输出）
4. `Docs/NN-<topic>/README.md` 已写（§5.2 七小节）
5. 适用时 ai-learn HTML 已生成
6. 涉及纯逻辑代码时 doctest 已写
7. 涉及可见输出时 golden PNG 已落盘
8. 本文件 §10 进度清单已勾

**Commit 规则**：
- 步内可有多次 WIP commit
- 每步结束强制一个 `step-NN: <topic>` 标题的总结性 commit
- 代码 + Docs 同时落盘

---

## §4 注释策略（覆盖默认）

**本仓库与默认相反：鼓励解释性注释。**

- 学习目标要求代码必须可读、可教学
- GPU / 同步 / 位运算 / 数学 / Bindless 等非显然概念**必须**写注释，解释 *为什么*、引用文档 / 论文 / Spec 章节
- shader 内部更应解释算法步骤（`// Step 1: 计算 cluster 边界 ... // Step 2: ...`）

**仍避免**：
- `// 自增 i` 这种复述代码做什么的废话
- 任务级 / PR 级注释（`// added for issue #123`）
- 多段 docstring 冗余

---

## §5 Docs 目录约定

### §5.1 目录结构
```
Docs/
  00-overview/
    README.md          # 子项目地图 + 全步骤索引
    glossary.md
  01-cmake-hello/
    README.md          # 中文精读笔记（核心）
    learn/             # 可选 — ai-learn HTML
    screenshots/       # 实机截图
    ue5-refs.md        # UE5 对照源码 file:line
  02-glfw-bringup/
  ...
  superpowers/
    specs/             # 设计文档
    plans/             # writing-plans 产物
```

### §5.2 每步 README.md 必含小节
1. 本步学了什么（1 段）
2. 为什么这么做（关键决策 + 替代方案）
3. 代码导读（指向具体 file:line）
4. UE5 是怎么做的（对应 file:line + 简述，若无则注明）
5. 截图 / GIF
6. 踩过的坑
7. 下一步预告

### §5.3 ai-learn 触发规则
| 类型 | 用 ai-learn |
|---|---|
| DX12 API 流程（device、swapchain、fence、heap、PSO...） | ❌ README + mermaid 时序图 |
| 多线程 / CPU↔GPU 同步模型 | ✅ |
| 算法可视化（prefix sum、HZB、cluster culling、SW raster、BVH、误差度量） | ✅ |
| 数学（投影矩阵、四元数、保守光栅...） | ✅ |

### §5.4 UE5 源码对照规则
- 每步必须列 UE5 对应实现的 `file:line`
- 引用记入 `Docs/NN-<topic>/ue5-refs.md`
- **不复制 UE5 代码**（License），仅引用 + 用自己的话复述
- 找不到对应时写"本步 harness 实现与 UE5 不直接对应"

---

## §6 构建 / 运行 / 调试 / 测试命令

### §6.1 构建（PowerShell）
```powershell
cmake --preset debug                   # 配置到 build-debug/
cmake --build build-debug -j           # 构建
cmake --preset release
cmake --build build-release -j
```

### §6.2 运行
```powershell
# 交互（人类）
./build-debug/wnanite.exe

# Headless（agent 用，步 20+ 可用）
./build-debug/wnanite.exe --headless --frames 60 --screenshot out/frame.png

# 指定 demo
./build-debug/wnanite.exe --demo hello-triangle --headless --frames 1 --screenshot out/tri.png
```

### §6.3 日志
- `logs/run-<timestamp>.log` — spdlog 主日志（含 D3D12 Debug Layer 重定向）
- `logs/dred-<timestamp>.json` — DRED GPU 崩溃 dump
- **Agent 一律读 log 文件**，不靠 stdout 截断

### §6.4 调试工具
| 工具 | agent 能用 | 用途 |
|---|---|---|
| D3D12 Debug Layer | ✅ 自动 | API 误用检测 |
| GPU-Based Validation | ✅ 自动 | shader UAV 越界 |
| DRED | ✅ 自动 dump | GPU 崩溃栈 |
| PIX UI | ❌ | 人类深度分析 |
| PIX CLI（pixtool） | ✅ 触发录制 | agent 录后告诉用户 `.wpix` 路径 |
| RenderDoc | ❌ | 人类用 |

### §6.5 测试
```powershell
ctest --test-dir build-debug --output-on-failure    # 单元测试
./scripts/visual-regression.ps1                      # 视觉回归（步 36+）
```

---

## §7 测试策略

**选择性 TDD + 视觉回归。**

| 层 | 工具 | 何时强制 |
|---|---|---|
| 纯逻辑（数学、bindless 槽位、RDG 依赖解析、SSIM...） | doctest | 必须 TDD |
| DX12 API 调用层 | 不写单测 | 跑 demo + golden image |
| Shader 算法（compute kernel 纯逻辑部分） | 自写 compute test runner | 选择性 |
| 端到端 | headless → PNG → SSIM | 每个可见输出步必须 |

**TDD 判断准则**：能在不创建 `D3D12Device` 的情况下测出来 → 必须 TDD。

**视觉回归 SSIM 阈值**：0.995（容忍字体抗锯齿 / imgui 排版微差）。

---

## §8 标准工作流

### §8.1 新学习步骤
```
brainstorming → writing-plans → 实现（含 TDD）
→ verification-before-completion（含截图 + 文档）
→ 写 Docs/NN-<topic>/README.md
→ 算法/同步/数学类：ai-learn HTML
→ step-NN commit
→ 更新 §10 进度
```

### §8.2 修 Bug
```
systematic-debugging → test-driven-development（写失败用例）
→ verification-before-completion
```

### §8.3 跨多步重构
```
brainstorming → writing-plans
→ dispatching-parallel-agents / subagent-driven-development
→ verification-before-completion → finishing-a-development-branch
```

---

## §9 禁止事项

- 不要跳过 `brainstorming` 直接写新功能代码
- 不要在没跑测试 / 没贴输出的情况下说"已完成"
- 不要用 `--no-verify`、`--no-gpg-sign` 等绕过 hook（除非用户显式要求）
- 不要在主分支 force push
- **不要复制 UE5 源码**（License 风险）
- 不要为"看起来在做事"省略 skill 流程——红旗思维（"这个很简单不用 skill"、"我记得这个 skill"）出现时立刻停下并调 skill
- **不要把一步做大**——若发现某步超过 1 小时或涉及多主题，停下来拆分

---

## §10 当前进度

> 完成定义见 §3。每完成一步勾选并在后面附 `Docs/NN-<topic>/README.md` 链接。

### 子项目 0：Harness（46 步）

**Phase A — Bootstrap**
- [x] 01 CMake 最小 hello-world — [Docs/01-cmake-hello](Docs/01-cmake-hello/README.md)
- [x] 02 引入 GLFW — [Docs/02-glfw-bringup](Docs/02-glfw-bringup/README.md)
- [x] 03 GLFW 窗口生命周期 — [Docs/03-window-lifecycle](Docs/03-window-lifecycle/README.md)
- [ ] 04 DXGI Factory + Adapter 枚举
- [ ] 05 D3D12 Device + Feature Level / SM 检测
- [ ] 06 Debug Layer + GPU-Based Validation
- [ ] 07 Command Queue + Fence 单帧
- [ ] 08 DXGI SwapChain
- [ ] 09 RTV 描述符堆
- [ ] 10 Command Allocator + List + 清屏
- [ ] 11 多帧 in-flight（双缓冲 fence）

**Phase B — Diagnostics**
- [ ] 12 spdlog
- [ ] 13 Debug Layer → spdlog 重定向
- [ ] 14 DRED + 崩溃 dump

**Phase C — Resources & Hello Triangle**
- [ ] 15 D3D12MA
- [ ] 16 Upload Heap helper
- [ ] 17 Root Signature 手写
- [ ] 18 PSO
- [ ] 19 Hello Triangle

**Phase D — Agent 自验证**
- [ ] 20 `--frames` + `--headless`
- [ ] 21 `--screenshot`
- [ ] 22 Smoke test 脚本

**Phase E — Shaders**
- [ ] 23 DXC 在线编译
- [ ] 24 DXC include handler + 从文件
- [ ] 25 文件监视 + 热重载

**Phase F — Bindless + UI + Profiling**
- [ ] 26 GPU 可见描述符堆
- [ ] 27 Bindless 槽位分配器
- [ ] 28 Bindless root signature
- [ ] 29 CB 环形分配
- [ ] 30 ImGui DX12 后端
- [ ] 31 GPU Timestamp Query

**Phase G — Profiling 工具 + 相机**
- [ ] 32 PIX Marker 包装
- [ ] 33 相机：透视/视图矩阵
- [ ] 34 相机：WASD + 鼠标

**Phase H — 测试基础设施**
- [ ] 35 doctest + 数学单测
- [ ] 36 Golden Image SSIM runner

**Phase I — RDG**
- [ ] 37 RDG v0 数据结构
- [ ] 38 RDG v0 执行
- [ ] 39 RDG v1 自动 barriers
- [ ] 40 RDG v2 生命周期 / aliasing

**Phase J — Shader 反射**
- [ ] 41 DXC Reflection
- [ ] 42 反射 → 自动 root signature

**Phase K — Mesh + Compute Demo**
- [ ] 43 cgltf
- [ ] 44 Hello Mesh
- [ ] 45 Prefix sum 单 pass
- [ ] 46 Prefix sum 多 pass on RDG

---

## §11 沟通约定

- 中文回复
- 工具调用前用一句话说明意图
- 结尾总结 1–2 句，只说改了什么、下一步是什么
- 引用代码用 `file_path:line_number` 格式
