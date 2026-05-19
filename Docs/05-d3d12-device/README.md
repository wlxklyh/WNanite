# 步 05 — D3D12 Device + Feature Level / SM 检测

> Phase A · Bootstrap · 第 5 步 / 共 11 步
> 对应 Spec：`Docs/superpowers/specs/2026-05-20-step-05-d3d12-device-design.md`
> **第一次创 `ID3D12Device`** —— Nanite 真正进场。

---

## 1. 本步学了什么

WNanite **第一次拿到 `ID3D12Device` 指针**——所有后续 D3D12 调用都从它开始。本步做：

- 在 DXGI 枚举循环里选**第一张非 software** adapter
- `D3D12CreateDevice(..., D3D_FEATURE_LEVEL_12_0, ...)` 创建设备
- `CheckFeatureSupport` 探测 8 个核心能力（FL / SM / Binding Tier / Heap Tier / Wave Ops / Atomic64 / Mesh Shader Tier / VRS Tier）
- 用 UE 实证的 Nanite 硬门槛 `SM >= 6.6 AND BindingTier >= 3` 做 gate

走完本步：stdout 含完整流水线 + `Nanite gate: PASS`；窗口仍可开/关。**设备拿到了**，下一步开 Debug Layer。

## 2. 为什么这么做

**为什么 MinFeatureLevel 是 `12_0` 不是 `12_2`**：

UE 实证（`D3D12Adapter.cpp:1350`）：UE Nanite 实际门槛是 `MaxFeatureLevel >= D3D_FEATURE_LEVEL_12_0 && SM >= 6.6 && BindingTier >= 3`——**Feature Level 不是 12_2**。

这是个反直觉点。Nanite 用的是 SM 6.6 + Bindless（Tier 3）的组合能力，这些**不绑定 Feature Level**——FL 是个粗糙的"硬件代际"标签，细粒度能力靠 `CheckFeatureSupport` 单独查。`D3D12CreateDevice` 的 `MinFeatureLevel` 用 `12_0` 就行，太严会把支持 SM 6.6 但 FL 报为 12_0 的硬件挡掉。

**为什么 SM 探测是"从高到低试"**：

`CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &data)` 的 `HighestShaderModel` 字段是 **in/out**：

- 输入：你"想问"的 SM（比如填 6.7）
- 输出：driver 实际支持到的 max SM（≤ 输入），或者 E_INVALIDARG 如果 driver 完全不识别这个常量

UE `WindowsD3D12Device.cpp:173 FindHighestShaderModel` 的策略：循环 SM 候选数组从高到低试，**第一个 SUCCESS 即设备 max**。直接填 `D3D_SHADER_MODEL_6_7` 一次性问 driver"你支持到几"是常见的初学者错误——某些旧 driver 不识别高版本常量会直接 E_INVALIDARG。

**为什么 Nanite 门槛是 SM 6.6 + Tier 3 同时成立**：

UE 注释（`D3D12Adapter.cpp:1076`）明确：

> `ResourceDescriptorHeap/SamplerDescriptorHeap must be supported on devices that support both D3D12_RESOURCE_BINDING_TIER_3 and D3D_SHADER_MODEL_6_6`

也就是**HLSL 里的 `ResourceDescriptorHeap[idx]` 这种 bindless 语法**，**必须**同时有 SM 6.6（编译器特性）和 Resource Binding Tier 3（运行时能力）。Nanite 大量用 bindless 访问 cluster buffer / texture / vertex 数据，缺一不可。

**为什么探测这 8 个能力**：

- **Max FL / SM**：基础门槛
- **Binding Tier / Heap Tier**：Bindless 必需
- **Wave Ops**：Nanite culling / SW raster 大量用 wave intrinsics
- **AtomicInt64**：SW rasterizer 写 64-bit visbuffer 的核心（高 32 位 depth + 低 32 位 triangle ID）
- **Mesh Shader Tier**：硬件光栅化分支可选用
- **VRS Tier**：步 05 用不到，但能力报告完整一点

本步只用 SM/Binding Tier 做 gate，其余打印作"能力档案"——下一步用到时不用重复查。

**为什么不开 Debug Layer**：

步 06 一起开 D3D12 Debug Layer + DXGI Debug Layer + GPU-Based Validation——三件套统一节奏。本步先用最干净的路径打通 device 创建。

## 3. 代码导读

main.cpp 长度从 ~165 行（步 04）扩到 ~400 行。主要分块：

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `src/main.cpp:7-9` | `<wrl/client.h>` / `<dxgi1_6.h>` / `<d3d12.h>` | DX12 头依赖 |
| `src/main.cpp:48-59` | `feature_level_str` | enum → "12.2" 等字符串 |
| `src/main.cpp:61-77` | `shader_model_str` | enum → "6.7" 等字符串 |
| `src/main.cpp:79-97` | `log_one_adapter` | 沿用步 04 打印格式，从 `log_dxgi_adapters` 拆出 |
| `src/main.cpp:107-148` | `pick_adapter` | 枚举 + 选第一张非 software |
| `src/main.cpp:151-172` | `create_device` | `D3D12CreateDevice` FL 12.0 |
| `src/main.cpp:188-278` | `query_caps` | 调用 7 次 `CheckFeatureSupport` |
| `src/main.cpp:210-233` | SM "从高到低试" 循环 | 与 UE `FindHighestShaderModel` 同模式 |
| `src/main.cpp:280-309` | `log_caps` | 打印能力报告（含 Mesh Shader Tier_1=10 enum 映射） |
| `src/main.cpp:311-326` | `check_nanite_gate` | SM>=6.6 ∧ Tier>=3 |
| `src/main.cpp:330-399` | main 流水线 | DXGI → pick → device → caps → gate → window |

## 4. UE5 是怎么做的

3 处关键引用（详见 `ue5-refs.md`）：

1. **`WindowsD3D12Device.cpp:173-200`** — `FindHighestShaderModel`：高到低试 SM 候选
2. **`D3D12Adapter.cpp:1076`** — 注释：`ResourceDescriptorHeap` 需要 `SM 6.6 AND Tier 3` 同时成立
3. **`D3D12Adapter.cpp:1350`** — Nanite 实际门槛代码：`FL >= 12_0 && SM >= 6.6 && BindingTier >= 3`

我们直接搬这套门槛——**对照 UE 看一遍，比看 MSDN 文档高效十倍**。

## 5. 截图 / GIF

实机 stdout（NVIDIA RTX 5060）：

```
Selected adapter: [0] NVIDIA GeForce RTX 5060
Creating D3D12 device at Feature Level 12.0 ... OK
Device capabilities:
    Max Feature Level:        12.2
    Max Shader Model:         6.7
    Resource Binding Tier:    3      [BINDLESS-CAPABLE]
    Resource Heap Tier:       2
    Wave Ops:                 yes
      Lane count:             32..32
    AtomicInt64 on typed RT:  yes
    Mesh Shader Tier:         1      [MESH-SHADER-CAPABLE]
    Variable Rate Shading:    Tier 2
Nanite gate (SM>=6.6 AND BindingTier>=3): PASS
```

RTX 5060 全部能力达标。

## 6. 深度学习点

### 6.1 `D3D12CreateDevice` 不必传 `IDXGIAdapter`

可以传 `nullptr` 让系统自选默认 adapter。但学习项目要可控——我们枚举完显式传，这样：
- 多 GPU 环境下选哪张是明确的
- 步 04 实测 NVIDIA 驱动会暴露同 GPU 多个 adapter，传 `nullptr` 可能选到非"主"adapter

### 6.2 `D3D12_FEATURE_DATA_SHADER_MODEL` 的 in/out 模式

这个结构体的 `HighestShaderModel` 字段**同时是输入和输出**：

```cpp
D3D12_FEATURE_DATA_SHADER_MODEL data{};
data.HighestShaderModel = D3D_SHADER_MODEL_6_7;  // 输入：我想问 6.7 是否支持
device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL, &data, sizeof(data));
// data.HighestShaderModel 现在是 driver 返回的 ≤6.7 的实际最高 SM
```

返回值的语义：

- `S_OK` + `data.HighestShaderModel <= 输入`：driver 知道这个 SM 值，告诉你实际能用到的最高
- `E_INVALIDARG`：driver 完全不认这个常量（太新）

所以 UE 的"从高到低循环"既能在新 driver 上一次命中，又能在旧 driver 上降级。

### 6.3 `D3D12_FEATURE_DATA_FEATURE_LEVELS` 的"候选列表"模式

这个不一样——它接受一个候选数组：

```cpp
D3D_FEATURE_LEVEL candidates[] = { 12_2, 12_1, 12_0, 11_1, 11_0 };
D3D12_FEATURE_DATA_FEATURE_LEVELS fls{};
fls.NumFeatureLevels = _countof(candidates);
fls.pFeatureLevelsRequested = candidates;
device->CheckFeatureSupport(D3D12_FEATURE_FEATURE_LEVELS, &fls, sizeof(fls));
// fls.MaxSupportedFeatureLevel = candidates 中最高的支持值
```

driver 一次性返回支持的最高 FL。**不像 SM 要循环**——两种 in/out 模式并存是 D3D12 API 设计的一致性瑕疵。

### 6.4 `CheckFeatureSupport` 的版本兼容性

Windows SDK 不断加新的 `D3D12_FEATURE_DATA_D3D12_OPTIONS<N>`：
- `OPTIONS`：Tier、Heap Tier
- `OPTIONS1`：Wave Ops
- `OPTIONS6`：VRS
- `OPTIONS7`：Mesh Shader
- `OPTIONS9`：AtomicInt64
- ...继续到 OPTIONS21+

**老 driver 不识别新结构体**，返回 `E_INVALIDARG`。所以本步全部用 `SUCCEEDED(...)` 包裹——失败保留默认值（0 / false），不影响后续逻辑。

这是 D3D12 渐进 API 的标准用法。

### 6.5 SDK 版本对 SM 检测的影响

本机 SDK 是 10.0.22621.0，`d3d12.h` 里 `D3D_HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_7`。即使 RTX 5060 硬件支持 SM 6.8 / 6.9，我们的代码也看不见——SDK 没定义这些常量。

要看见 6.8+ 有两条路：
1. **升级 Windows SDK** 到 10.0.26100+（系统级）
2. **集成 D3D12 Agility SDK**（推荐，UE5 也用）：自带新 d3d12.h，可在任意 Windows 上启用最新 D3D12 特性

学习项目暂时用 SDK 默认——Nanite gate 卡在 6.6，看到 6.7 已足够。

### 6.6 Wave Ops 与 Nanite SW Rasterizer 的关系

Nanite SW raster 的核心是：

```hlsl
// 每个 wave 处理一个 cluster 的 N 个三角形
for (uint tri = WaveGetLaneIndex(); tri < cluster.NumTriangles; tri += WaveGetLaneCount())
{
    // 光栅化这一个三角形
    ...
    // 用 InterlockedMax/Min 写 visbuffer（64-bit atomic）
    InterlockedMax(VisBuffer[pixel], packed64);
}
```

依赖：
- `WaveGetLaneIndex` / `WaveGetLaneCount`：wave intrinsics
- `InterlockedMax` 在 64-bit typed UAV 上：AtomicInt64 typed

两个都是步 05 探测的能力。**没有这俩，SW raster 写不了**。

### 6.7 与 UE 对照 — 我们和 UE 用一样的门槛

UE 的代码（精简）：

```cpp
// D3D12Adapter.cpp:1350
if (Desc.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_12_0
    && Desc.MaxSupportedShaderModel >= D3D_SHADER_MODEL_6_6
    && Desc.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3)
{
    // Nanite-capable
}
```

我们的：

```cpp
const bool sm_ok    = caps.max_shader_model >= D3D_SHADER_MODEL_6_6;
const bool tier_ok  = caps.binding_tier     >= 3;
return sm_ok && tier_ok;
```

**完全同模式**——少了 FL 12_0 检查是因为我们传 `D3D12CreateDevice` 的 MinFeatureLevel 就是 12_0，能创出 device 就已经过 FL 门槛。

## 7. 踩过的坑

### 7.1 `D3D12_MESH_SHADER_TIER_1 = 10`（实战发现）

**本步执行中真撞上的 bug**——按"raw integer"打印 mesh shader tier 输出：

```
Mesh Shader Tier:         10      [MESH-SHADER-CAPABLE]
```

`Mesh Shader Tier: 10` 看起来非常怪。查 `d3d12.h` 才知：

```cpp
typedef enum D3D12_MESH_SHADER_TIER
{
    D3D12_MESH_SHADER_TIER_NOT_SUPPORTED = 0,
    D3D12_MESH_SHADER_TIER_1             = 10   // ← 不是 1
} D3D12_MESH_SHADER_TIER;
```

**MS 在 enum 值里跳到 10**——大概是给未来 Tier 2/3/5/7 留 enum 空间，避免连续小整数撞 ABI。其他 D3D12 tier enum（Resource Binding / Heap / VRS）都是连续 1/2/3，**只有 Mesh Shader 是 10**。

修复：raw enum 值 0 / 10 显式映射到 `NOT_SUPPORTED` / `"1"` 输出，gate 判定从 `>= 1` 改 `>= 10`（或 `>= D3D12_MESH_SHADER_TIER_1`）。

**教训**：D3D12 enum 不要假定"连续小整数"。打印 raw int 之前先查头文件确认值域。

### 7.2 SDK 10.0.22621 最高 SM = 6.7

引用 `D3D_SHADER_MODEL_6_8` / `_6_9` 直接编译错。spec 自审阶段查 `C:\Program Files (x86)\Windows Kits\10\Include\10.0.22621.0\um\d3d12.h` 看到 `D3D_HIGHEST_SHADER_MODEL = D3D_SHADER_MODEL_6_7` 才发现。

**教训**：spec 写代码前先验证 SDK 实际定义。

### 7.3 `D3D12CreateDevice` 失败的两种原因

- adapter 不达 MinFeatureLevel → `E_NOINTERFACE` 等
- adapter 是 software 但 D3D12 runtime 不支持 → 不同错误码

我们提前 `pick_adapter` 跳过 software，所以本步只可能撞第一种。

### 7.4 ComPtr 在异常 / early return 的安全性

```cpp
ComPtr<ID3D12Device> device = create_device(adapter);
if (!device) { glfwTerminate(); return 1; }
```

`create_device` 失败时返回空 `ComPtr`，**析构无副作用**（不 Release nullptr）。RAII 在多 early return 的流水线里就是这种"不用想"的安全感——裸指针 + 手动 Release 写这个流水线得手动 cleanup 5 次。

### 7.5 不同 D3D12_FEATURE 用不同结构体大小校验

`CheckFeatureSupport(feature, &data, sizeof(data))` 第三个参数 driver 用来匹配版本——传错大小会 `E_INVALIDARG`。`sizeof(data)` 不能省。

### 7.6 `D3D12_RESOURCE_BINDING_TIER_3 = 3` 是真整数

`ResourceBindingTier` 是连续 enum 1/2/3。我们直接 `binding_tier >= 3` 比较没问题。这点和 Mesh Shader Tier_1=10 形成对比——**同一组 D3D12 API 内部命名一致但值域不一致**。这就是 7.1 容易撞的根本原因。

### 7.7 NVIDIA driver 同 GPU 多 adapter 在 step 04 已识别

步 04 实测 NVIDIA 给同张 RTX 5060 暴露 3 个 DXGI adapter（不同 LUID）。步 05 选第一张非 software，会选到 `[0]` 这张主独显。但如果有诡异机器配置（比如机型 BIOS 把 iGPU 排前），可能选到核显——本项目不去重，未来 imgui 接入后做选择 UI。

## 8. 下一步预告

**步 06：Debug Layer + GPU-Based Validation**

三件套统一开：

- DXGI Debug Layer（`DXGIGetDebugInterface1`）
- D3D12 Debug Layer（`D3D12GetDebugInterface`）
- GPU-Based Validation（在 D3D12 Debug 之上加 shader UAV 越界等运行时检查）

故意 API 误用（比如不带必需 `D3D12_RESOURCE_STATE_*` 调 `ResourceBarrier`）能在 OutputDebugString 看到详细错误。**对 DX12 开发是必备工具**。
