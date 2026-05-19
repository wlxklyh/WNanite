# UE5 源码对照 — 步 05

按 CLAUDE.md §1 "遇事不决查 UE5" 规则记录。

## SM 探测：从高到低试

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Device.cpp:173-200`
  - `FindHighestShaderModel(ID3D12Device* Device)` 静态函数
  - 内部循环 `D3D_SHADER_MODEL` 候选数组（从高到低）
  - 每次填 `FeatureShaderModel.HighestShaderModel = ShaderModelToCheck` 调 `CheckFeatureSupport`
  - 第一个 SUCCEEDED 返回 `FeatureShaderModel.HighestShaderModel`
  - **WNanite 直接搬这个模式**（`query_caps` 里的 `sm_candidates` 循环 — `src/main.cpp:210-233`）

## Bindless 与 SM 6.6 / Tier 3 的关系

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\D3D12Adapter.cpp:1076`
  - 注释原文：`ResourceDescriptorHeap/SamplerDescriptorHeap must be supported on devices that support both D3D12_RESOURCE_BINDING_TIER_3 and D3D_SHADER_MODEL_6_6`
  - **解读**：HLSL 里 `ResourceDescriptorHeap[idx]` 这种动态索引 bindless 语法，**编译器**侧需要 SM 6.6 支持，**运行时**侧需要 Tier 3 资源绑定支持，缺一不可
  - WNanite 把这两条 AND 起来作为 Nanite gate（`src/main.cpp:311-326 check_nanite_gate`）

## Nanite 真实门槛

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\D3D12Adapter.cpp:1350`
  - 原文：`if (Desc.MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_12_0 && Desc.MaxSupportedShaderModel >= D3D_SHADER_MODEL_6_6 && Desc.ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3)`
  - **三条 AND**：FL ≥ 12_0、SM ≥ 6.6、BindingTier ≥ 3
  - WNanite 简化：FL 12_0 已经在 D3D12CreateDevice 的 MinFeatureLevel 卡住，所以 `check_nanite_gate` 只显式检查 SM + Tier

## Adapter Desc 缓存

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\D3D12Adapter.h:149`
  - `FORCEINLINE D3D_SHADER_MODEL GetHighestShaderModel() const { return Desc.MaxSupportedShaderModel; }`
  - UE 把 `MaxSupportedShaderModel` 缓存在 `Desc`（`FD3D12AdapterDesc`）里
  - **WNanite 简化**：我们直接传 `DeviceCaps` struct，不另开 adapter 抽象。等真要支持多 device 时再做

## 设计哲学差异

| 点 | UE D3D12RHI | WNanite |
|---|---|---|
| Adapter / Device 封装 | `FD3D12Adapter` + `FD3D12Device` 类 | 直接 ComPtr，main 持有 |
| Adapter 选择 | 配置/命令行/UI | 隐式第一张非 software |
| Feature 缓存 | `FAdapterDesc` 缓存所有探测结果 | `DeviceCaps` 一次性 struct |
| 错误恢复 | RHI 层故障转移 | 直接 exit 1 |
| 多 device | 支持（多 GPU 渲染） | 单 device |

**注意**：本仓库不复制 UE5 源码（License）。所有 file:line 仅作对照参考。
