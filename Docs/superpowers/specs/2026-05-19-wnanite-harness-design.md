# WNanite Harness 设计文档（子项目 0）

**日期**：2026-05-19
**作者**：linyanhou98@gmail.com + Claude（brainstorming）
**状态**：草案（待用户审阅）
**范围**：仅 Harness（子项目 0）。Nanite 算法实现见后续子项目。

---

## 1. 目标与范围

### 1.1 项目级目标
WNanite 是一个**学习项目**：从零写 DX12 沙盒，复刻 UE5 Nanite 大部分流程，用于深入理解 DX12 与 Nanite 算法。优先级永远是 **理解 > 速度 > 性能**。

### 1.2 子项目 0（本文档）目标
为后续 Nanite 算法子项目搭好**中型 harness**——一个可以承载 ≥20 个 compute pass + 多个 graphics pass 的最小可行渲染基础设施。

### 1.3 子项目 0 范围内
- DX12 设备 / SwapChain / 命令列表 / 同步
- 资源管理（D3D12MA）、Bindless 描述符堆
- DXC 在线编译 + 文件监视热重载 + 反射→自动 root signature
- 轻量 RDG（Pass / Resource 声明 + 自动 barrier + 资源生命周期/aliasing）
- GPU profiling（timestamp + PIX marker）
- ImGui DX12 后端
- 自由飞行相机 + 输入
- cgltf glTF 加载 + 基础 mesh GPU 上传
- Compute 多 pass demo（prefix sum）验证 RDG/bindless
- Headless / `--screenshot` / `--frames N` 支持（agent 可自验证）
- 测试：选择性 TDD（doctest）+ Golden Image SSIM 视觉回归

### 1.4 子项目 0 范围外（后续子项目）
- Cluster / Meshlet 数据结构与 cooker（子项目 1）
- Two-level culling + HZB + Mesh Shader / 传统 raster（子项目 2）
- Software Rasterizer + 64-bit visibility buffer（子项目 3）
- Visibility buffer 着色 / Material pass（子项目 4）
- LOD 选择 + DAG 遍历（子项目 5）
- Streaming / Virtual Geometry / 异步 IO 与 residency（子项目 6）

---

## 2. 整体子项目分解

| # | 子项目 | 关键产出 | 依赖 |
|---|---|---|---|
| **0** | **Harness（本文档）** | 见 §1.3 | — |
| 1 | Cooker + Cluster 构建 | 离线工具 glTF → meshlet → DAG → 二进制 | 0 |
| 2 | Cluster 渲染 MVP（HW raster） | 两级 culling + HZB occlusion + 传统/Mesh Shader 绘制 | 0, 1 |
| 3 | SW Rasterizer | persistent thread compute + 64-bit visibility buffer | 0, 2 |
| 4 | Visibility Buffer 着色 | Material pass、deferred 评估 | 0, 3 |
| 5 | LOD + DAG 遍历 | 屏幕误差驱动的 cut selection | 0, 1, 2 |
| 6 | Streaming / Virtual Geometry | 页式加载、residency、异步 IO | 0, 1, 5 |

每个子项目独立 `brainstorming → spec → writing-plans → 实现 → verification → review`。

---

## 3. 已锁定的技术决策

| 维度 | 决定 | 理由 |
|---|---|---|
| Harness 类型 | 纯 DX12 沙盒，非 UE5 项目 | 学习目标要求完全可控 |
| 平台 | Windows 11 + DX12 only | 项目目标即 DX12 |
| 构建系统 | CMake（≥ 3.25，preset） | 生态最广、IDE 通用、AI 工具熟 |
| 编译器 | MSVC（Visual Studio 2022） | DX12 原生工具链 |
| C++ 标准 | C++20 | concepts / ranges / consteval 使代码更清晰 |
| Shader Model | SM 6.6（最低门槛） | Bindless ResourceDescriptorHeap + AtomicInt64 + wave ops，Nanite 必需 |
| Shader 编译 | DXC（DirectXShaderCompiler） | SM 6.6+ 必须用 DXC |
| 窗口/输入 | GLFW（FetchContent） | 轻量、imgui 适配好 |
| GPU 内存分配 | D3D12MemoryAllocator (D3D12MA) | AMD 官方、生产级 |
| glTF 解析 | cgltf | 单头文件、零依赖 |
| Mesh 工具（后置） | meshoptimizer（子项目 1 用） | Nanite 风格 meshlet 必需 |
| UI | Dear ImGui + dx12/glfw backend | 事实标准 |
| 日志 | spdlog | 性能好、API 友好 |
| 单元测试 | doctest | 头文件单一、编译快 |
| 数学 | DirectXMath（首选） | 与 D3D12 原生匹配，SIMD 友好 |
| Mesh Shader | 可选（运行时检测） | 留给子项目 2 决定走 SW 还是 Mesh |

---

## 4. Harness 架构

### 4.1 模块边界

```
src/
  Core/           App / Window / Input / Time / Logger / CmdLine
  RHI/            Device / Adapter / Queue / Fence / SwapChain / Frame
  Resource/       D3D12MA 包装、Buffer/Texture handle、Upload Ring
  Descriptor/     CPU/GPU 描述符堆、Bindless 槽位分配器
  Shader/         DXC Compiler、IncludeHandler、HotReload、Reflection
  RootSig/        Bindless 根签名 + 反射生成
  RDG/            PassBuilder、ResourceDesc、Compiler、Executor
  Pipeline/       PSO 缓存
  Profile/        GPU Timestamp 池 + PIX marker 包装
  Imgui/          DX12 后端胶水
  Camera/         透视/视图矩阵 + WASD/鼠标控制
  Mesh/           cgltf → CPU mesh → GPU upload
  Test/           doctest runner、Golden Image SSIM
  Headless/       --frames / --screenshot / offscreen target
  App.cpp         main + 命令行解析

shaders/          *.hlsl + 共享头
tests/            *.cpp（doctest）
golden/           参考 PNG（视觉回归）
Docs/             学习产物（见 §6）
```

### 4.2 数据流（典型一帧）

```
Frame Begin
  → Headless? 切 offscreen RT : 取 swapchain backbuffer
  → 收集本帧 imgui 输入（GLFW callback）
  → 相机更新（输入 → matrices → root CBV）
  → RDG.Begin()
      → 业务 pass 注册（compute / graphics）
      → 资源声明（Read/Write/UAV/Create）
  → RDG.Compile()   // 拓扑序、生命周期、aliasing、自动 barrier
  → RDG.Execute(cmdList)
      → 每 pass 前后插入 GPU timestamp
      → 每 pass 包 PIX marker
  → ImGui Render Pass（直接上 backbuffer）
  → cmdList.Close + Queue.ExecuteCommandLists
  → SwapChain.Present
  → Headless? 截图 + 帧计数 → 达到 --frames 退出
  → Fence Signal + 下一帧 Wait
Frame End
```

### 4.3 关键设计选择

**Bindless 风格**
- 单一巨大 CBV/SRV/UAV 描述符堆（启动时分配 1M 槽位）
- Sampler 单独一个堆（≤2048 上限）
- 根签名只包含：少量 root CBV（per-frame、per-draw）+ root constants（≤64 DWORD）
- shader 用 `ResourceDescriptorHeap[idx]` / `SamplerDescriptorHeap[idx]` 访问
- 槽位分配：栈式 free list，handle = 32-bit index + generation counter（防 use-after-free）

**RDG 抽象层级（三阶段渐进）**
- v0（步 37-38）：仅 pass + resource 声明，串行执行，无自动 barrier
- v1（步 39）：在 v0 上加自动 transition / UAV barrier
- v2（步 40）：在 v1 上加 first/last use 分析 + 资源 aliasing（显存复用）
- 不做：异步 compute queue、async copy queue（留给后续子项目按需引入）

**同步模型**
- 单 DIRECT queue（Nanite 主路径同框架够用）
- 双缓冲帧 fence（in-flight=2）
- 每帧一个 command allocator，allocator 在 fence 完成后 reset

---

## 5. 学习步骤拆分（46 步）

每步约束：
- 工作量 < 1 小时
- 单一主题
- 编译通过 + 运行通过 + 可见或可测的产出
- 配套 `Docs/NN-<topic>/` 文档（细节见 §6）
- 完成定义见 §10.1

### Phase A — Bootstrap（11 步）

| # | 主题 | 验收 |
|---|---|---|
| 01 | CMake 最小 hello-world | 编译输出 "hello" |
| 02 | 引入 GLFW（FetchContent） | 链接通过 |
| 03 | GLFW 窗口生命周期 + 事件循环 | 蓝色窗口可关 |
| 04 | DXGI Factory + Adapter 枚举 + 日志 | log 列出所有适配器 |
| 05 | D3D12 Device 创建 + Feature Level / SM 检测 | log 输出 SM 6.6 等能力 |
| 06 | Debug Layer + GPU-Based Validation 开关 | 故意错用 API 看到报错 |
| 07 | DIRECT Command Queue + Fence 单帧同步 | 一次 Signal/Wait 跑通 |
| 08 | DXGI SwapChain（Flip Discard） | 颜色循环清屏 |
| 09 | RTV 描述符堆 + 创建 RTV | ClearRenderTargetView |
| 10 | Command Allocator + Command List + 清屏命令 | 清屏走真实命令录制 |
| 11 | 多帧 in-flight（双缓冲 fence 模型） | 不卡 + 不撕裂 |

### Phase B — Diagnostics（3 步）

| # | 主题 | 验收 |
|---|---|---|
| 12 | spdlog 集成（文件 + stderr 双 sink） | `logs/run-*.log` 出现 |
| 13 | D3D12 Debug Layer 输出 → spdlog 重定向 | Debug Layer 错误进 log |
| 14 | DRED 启用 + GPU 崩溃 dump 到 JSON | 故意 device removed 看到栈 |

### Phase C — Resources & Hello Triangle（5 步）

| # | 主题 | 验收 |
|---|---|---|
| 15 | D3D12MA 集成 + 第一个 GPU buffer | 创建/销毁不泄漏 |
| 16 | Upload Heap + CPU→GPU 拷贝 helper | 上传简单数据 |
| 17 | Root Signature 手写最小版（1 root CBV） | 拿到 ID3D12RootSignature |
| 18 | PSO 创建（graphics） | 拿到 ID3D12PipelineState |
| 19 | 顶点 buffer + IA + Hello Triangle | 三角形 |

### Phase D — Agent 自验证（3 步，提前到此处）

> 引入 headless / 截图早于 RDG / Bindless / Mesh 等大模块，让 Agent 从此**每一步都能自动跑 + 截图验证**，而不是一直靠人肉看窗口。

| # | 主题 | 验收 |
|---|---|---|
| 20 | `--frames N` + `--headless`（offscreen RT） | 不开窗口跑完 N 帧退出 |
| 21 | `--screenshot <path>`（backbuffer / offscreen → PNG） | 三角形 PNG 落盘 |
| 22 | Smoke test：CI 风格脚本跑通三角形 → PNG | exit code = 0 + PNG 存在 |

### Phase E — Shaders（3 步）

| # | 主题 | 验收 |
|---|---|---|
| 23 | DXC 在线编译（DxcCompiler API） | 从 .hlsl 字符串编译 |
| 24 | DXC IDxcIncludeHandler + 从文件加载 | 多文件 shader 跑通 |
| 25 | 文件监视（efsw 或 ReadDirectoryChangesW）+ shader 热重载 | 改 hlsl 即时刷新画面 |

### Phase F — Bindless + UI + Profiling（6 步）

| # | 主题 | 验收 |
|---|---|---|
| 26 | GPU 可见 CBV/SRV/UAV 描述符堆（1M 槽位） | 堆创建 + 基础设施 |
| 27 | Bindless 槽位分配器（栈式 free list + generation） | imgui 显示槽位使用率 |
| 28 | Bindless root signature + ResourceDescriptorHeap shader 写法 | hello triangle 改 bindless |
| 29 | Constant Buffer 环形分配器（per-frame） | 多帧无竞争 |
| 30 | ImGui DX12 后端接入 | demo 面板可点 |
| 31 | GPU Timestamp Query 池 + 解算 + imgui 显示 | imgui 显示每 pass µs |

### Phase G — Profiling 工具 + 相机（3 步）

| # | 主题 | 验收 |
|---|---|---|
| 32 | PIX Marker / PIXBeginEvent 包装（含 PIXEventsThreadInfo） | PIX capture 有事件树 |
| 33 | 相机：透视/视图矩阵 + uniform 上传 | imgui 调 FOV 起作用 |
| 34 | 相机：WASD + 鼠标右键拖拽视角 | 可飞行 |

### Phase H — 测试基础设施（2 步）

| # | 主题 | 验收 |
|---|---|---|
| 35 | doctest 接入 + CMake CTest + 数学/工具单测 | `ctest` 全绿 |
| 36 | Golden Image SSIM runner（阈值 0.995） | 故意改像素能检出 |

### Phase I — RDG（4 步）

| # | 主题 | 验收 |
|---|---|---|
| 37 | RDG v0：PassBuilder / ResourceDesc 数据结构 | 单元测试覆盖声明 API |
| 38 | RDG v0：拓扑排序 + 串行执行（手动 barrier） | hello triangle 跑在 RDG 上 |
| 39 | RDG v1：自动 transition / UAV barrier 推导 | 移除所有手写 barrier 调用 |
| 40 | RDG v2：first/last use + aliasing（显存复用） | 物理显存峰值下降 |

### Phase J — Shader 反射（2 步）

| # | 主题 | 验收 |
|---|---|---|
| 41 | DXC Reflection API 接入 | log 输出 binding 表 |
| 42 | 反射 → 自动生成 / 校验 root signature | shader 改了 RS 跟着对 |

### Phase K — Mesh + Compute Demo（4 步）

| # | 主题 | 验收 |
|---|---|---|
| 43 | cgltf 集成 + 解析 mesh 数据 | log 输出顶点 / 三角形数 |
| 44 | Mesh GPU 上传（VB/IB）+ "Hello Mesh"（一个 glTF 模型可见） | 模型出现在屏幕上 |
| 45 | Compute Demo：prefix sum 单 pass + 读回校验 | 单元测试比对 |
| 46 | Compute Demo：多 pass prefix sum on RDG | RDG 自动 barrier 正确 |

### 5.1 关键节点
- **步 19**：第一次可见画面
- **步 21**：Agent 开始能自验证
- **步 25**：进入 "改 shader 不重启" 节奏
- **步 31**：能看到 GPU 性能数据
- **步 36**：视觉回归框架可用
- **步 40**：RDG 三阶段完成
- **步 46**：Harness 完结，可进入子项目 1

---

## 6. Docs/ 目录与学习产物约定

### 6.1 目录结构
```
Docs/
  00-overview/
    README.md                    # 子项目地图 + 全步骤索引
    glossary.md                  # 术语表
  01-cmake-hello/
    README.md                    # 中文精读笔记
    learn/                       # 可选 — ai-learn HTML 产物
    screenshots/                 # 实机截图
    ue5-refs.md                  # UE5 对照源码 file:line（若适用）
  02-glfw-bringup/
  ...
  superpowers/
    specs/                       # 本设计文档所在地
    plans/                       # writing-plans 产物
```

### 6.2 每步 README.md 必含小节
1. **本步学了什么**（1 段）
2. **为什么这么做**（关键决策 + 替代方案）
3. **代码导读**（指向具体 file:line）
4. **UE5 是怎么做的**（对应 UE5 源码 file:line + 简要说明，若无则注明 "本步与 UE5 无直接对应"）
5. **截图 / GIF**
6. **踩过的坑**
7. **下一步预告**

### 6.3 ai-learn 触发规则
| 类型 | 用 ai-learn |
|---|---|
| DX12 API 流程（device、swapchain、fence、heap、PSO...） | ❌ README + 时序图（mermaid）足够 |
| 多线程 / CPU↔GPU 同步模型 | ✅ Three.js 时间轴交互 |
| 算法可视化（prefix sum、HZB、cluster culling、SW raster 扫描线、BVH、误差度量） | ✅ 必须 |
| 数学（投影矩阵、四元数、保守光栅...） | ✅ |

Harness 阶段（步 01-46）预计**只有少数几步需要 ai-learn**：
- 步 11（多帧 in-flight 模型）— CPU/GPU pipeline 可视化
- 步 33（投影/视图矩阵）— 数学可视化
- 步 36（SSIM）— 误差度量
- 步 45-46（prefix sum）— 算法可视化

其余步骤手写 README + mermaid 图就够。

### 6.4 UE5 源码对照规则
- 学习时必须列出 UE5 对应实现的 `file:line`（如 `Engine/Source/Runtime/D3D12RHI/Private/D3D12Device.cpp:123`）
- 引用记入 `Docs/NN-<topic>/ue5-refs.md`
- **不复制 UE5 代码**（License）；仅引用 + 用自己的话复述
- 找不到对应时明确写 "本步 harness 实现与 UE5 不直接对应"

---

## 7. 测试策略

### 7.1 分层
| 层 | 用什么 | 何时强制 |
|---|---|---|
| 纯逻辑（数学、bindless 槽位、RDG 依赖解析、SSIM 算法...） | doctest | 必须 TDD |
| DX12 API 调用层 | 不写单测 | 靠运行 demo + golden image |
| Shader 算法（compute kernel 的纯逻辑部分） | 自写 compute test runner：跑 → 读回 → 比对 | 选择性 |
| 端到端 | headless 跑 demo → 出 PNG → SSIM 比对 + 检查 log 无 ERROR | 每个有可见输出的步必须 |

### 7.2 TDD 适用范围
- **必须 TDD**：步 35 之后的所有"纯逻辑"代码（RDG 调度器、bindless 分配器、SSIM、相机数学等）
- **不写单测**：DX12 API wrapper、PSO 创建、shader 热重载这类强依赖 GPU/驱动的代码
- **判断准则**：能在不创建 D3D12Device 的情况下测出来 → 必须写

### 7.3 视觉回归
- 每个有可见输出的步在 `golden/NN-<topic>.png` 留参考
- `--screenshot` 模式跑 → 与 golden 比 SSIM
- 阈值 0.995（足够宽容字体抗锯齿、imgui 排版差异）
- 失败时 dump diff PNG 到 `logs/diff-*.png`

### 7.4 测试运行
- `ctest` 跑单元测试
- `scripts/visual-regression.ps1` 跑视觉回归
- 两者都进 CLAUDE.md 的"验证命令"段，agent 不用问

---

## 8. Agent 环境约定

### 8.1 构建命令
```powershell
cmake --preset debug                  # 配置（生成到 build-debug/）
cmake --build build-debug -j          # 构建
cmake --preset release
cmake --build build-release -j
```

### 8.2 运行命令
```powershell
# 交互模式（人类）
./build-debug/wnanite.exe

# Headless 模式（agent）
./build-debug/wnanite.exe --headless --frames 60 --screenshot out/frame.png

# 指定 demo（步 19 之后有 --demo 标志）
./build-debug/wnanite.exe --demo hello-triangle --headless --frames 1 --screenshot out/tri.png
```

### 8.3 日志
- `logs/run-<timestamp>.log` — spdlog 主日志（含 Debug Layer 重定向）
- `logs/dred-<timestamp>.json` — GPU 崩溃时由 DRED 写出
- agent 一律读 log 文件，不靠 stdout 截断

### 8.4 调试工具
| 工具 | agent 能用 | 用途 |
|---|---|---|
| D3D12 Debug Layer | ✅（自动） | API 误用检测 |
| GPU-Based Validation | ✅（debug build 自动） | shader UAV 越界等 |
| DRED | ✅（自动 dump） | GPU 崩溃栈 |
| PIX (UI) | ❌（agent 看不了 .wpix） | 人类深度分析 |
| PIX CLI（pixtool） | ✅（触发录制） | agent 录制后告诉用户 .wpix 路径 |
| RenderDoc | ❌（同 PIX） | 人类用 |

### 8.5 截图
- 内置 `--screenshot <path>` 为首选
- 备选：PowerShell 调 Win32 API（DPI 麻烦，不推荐）

### 8.6 单元测试 & 视觉回归命令
```powershell
ctest --test-dir build-debug --output-on-failure
./scripts/visual-regression.ps1     # 运行所有 golden 比对
```

### 8.7 Telegram 通道（已存在）
- agent 长任务完成后 reply Telegram 提醒（已在 plugin:telegram 配置）
- 适用场景：cooker 长跑、CI 视觉回归批跑

---

## 9. 错误处理

### 9.1 启动期失败
- 适配器枚不到 / 不支持 SM 6.6 / 不支持 Bindless → 打印能力表 + 退出码 != 0
- DXC 加载失败 → 同上
- Shader 编译失败 → 打印 DXC 完整错误 + 不进入主循环

### 9.2 运行期失败
- 资源分配失败（D3D12MA OOM）→ log ERROR + 优雅退出
- Device Removed → 触发 DRED dump + 友好提示 + 退出
- Shader 热重载失败 → imgui 覆盖层红色提示 + 保留旧 PSO 继续渲染
- RDG 校验失败（资源未声明 / 循环依赖）→ assert + log + 退出（debug build）

### 9.3 测试期失败
- 单元测试失败 → ctest 非零退出，CI 标红
- 视觉回归失败 → 写 diff PNG 到 `logs/diff-*.png` + 退出码 != 0

---

## 10. 退出条件 / 验收标准

### 10.1 单步完成定义
一步算完成当且仅当：
1. 编译通过（Debug + Release）
2. 运行通过（headless 或交互）
3. 有验收信号（截图 / 日志 / 测试输出）
4. `Docs/NN-<topic>/README.md` 已写（覆盖 §6.2 七小节）
5. 适用时 ai-learn HTML 已生成
6. 涉及纯逻辑代码时 doctest 已写
7. 涉及可见输出时 golden PNG 已 commit
8. CLAUDE.md 中"已完成步骤"清单已更新

### 10.2 子项目 0 完成定义
全部 46 步完成 + 以下额外条件：
1. `ctest` 全绿
2. 全部 golden image 视觉回归通过
3. 可在 4K 显示器 60fps 渲染 Hello Mesh + ImGui + GPU 时间戳面板
4. PIX capture 显示完整事件树 + 每 pass timestamp
5. `Docs/00-overview/README.md` 有完整步骤索引

---

## 11. CLAUDE.md 同步更新清单（本设计配套）

随本设计文档落地后，`CLAUDE.md` 需补充：
1. **项目目标段** — 学习导向 / 子项目分解
2. **学习步骤约定** — 1 步 = 1 主题 = 1 Docs 目录；过程中可有多次 WIP commit，每步结束时强制一个 `step-NN: <topic>` 标题的总结性 commit，对应代码 + 文档同步落盘
3. **注释策略覆盖** — 学习代码鼓励解释性注释（与默认相反，必须显式声明）
4. **UE5 源码对照规则** — file:line 引用方式 + 不复制 UE5 代码
5. **Docs 目录约定** — §6.1 / 6.2 / 6.3 / 6.4 全部
6. **ai-learn 触发规则** — §6.3 表格
7. **构建 / 运行 / 调试 / 测试命令** — §8 全部
8. **测试策略** — §7 全部 + TDD 适用范围
9. **Skill 绑定表更新** — 加入 ai-learn（按 §6.3 规则）
10. **当前进度跟踪** — 已完成步骤清单（每完成一步追加）

---

## 12. 风险与未决

- **风险 A：DXC 反射对 bindless root signature 的支持深度**
  - 缓解：步 41-42 前先做 PoC，若反射不够用则 fallback 到 shader 注释式声明
- **风险 B：RDG aliasing 可能出非平凡 bug**
  - 缓解：v0 / v1 / v2 分三步上线，v2 可延后到子项目 2 再做
- **风险 C：efsw 文件监视在 Windows 文件锁场景行为**
  - 缓解：若不稳改用 ReadDirectoryChangesW
- **未决**：CI 是否引入（GitHub Actions Windows runner）→ 子项目 0 末再决定，先把 `scripts/visual-regression.ps1` 写好
- **未决**：是否在 Phase F 加入 ResourceBarrier 批处理优化 → 默认不做（v0 简单串行），如性能瓶颈出现再回填

---

## 13. 下一步流程

1. **本文档** → 用户审阅 → 修订（若需要）
2. 用户审阅通过 → 同步更新 `E:\LYH\WNanite\CLAUDE.md`（§11 清单）
3. 调用 `superpowers:writing-plans` → 产出 `Docs/superpowers/plans/2026-05-19-wnanite-harness-plan.md`
4. 实施按 §5 步骤顺序，每步遵守 §10.1 完成定义
5. 步 46 完成后进入子项目 1 brainstorming

---

*本文件未提交到 git（仓库目前不是 git 项目）。第一步实施前会执行 `git init`，本文件作为首个 commit 的一部分。*
