# UE5 源码对照 — 步 04

按 CLAUDE.md §1 "遇事不决查 UE5" 规则记录的对照引用。

## Adapter 枚举主入口

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Device.cpp:545-571`
  - `TArray<TRefCountPtr<IDXGIAdapter>> DXGIAdapters` 存所有枚到的 adapter
  - 内层用老 API `DXGIFactoryForDisplayList->EnumAdapters(AdapterIndex, TempAdapter.GetInitReference())`
  - **类比 WNanite**：`ComPtr<IDXGIAdapter1> adapter; factory->EnumAdapterByGpuPreference(...)`

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Device.cpp:813-863`
  - `TRefCountPtr<IDXGIFactory4> DXGIFactory4` + `TRefCountPtr<IDXGIFactory6> DXGIFactory6`
  - `FD3D12AdapterDesc::EnumAdapters(...)` 包装 Factory4/6 双路径
  - **WNanite 简化**：只用 Factory6（学习项目门槛卡 Win10 1803+）

## Adapter 描述

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\D3D12Adapter.h:28+`
  - `class FD3D12Adapter` 包装 `IDXGIAdapter` + 它的描述 / 能力探测
  - 后续步 05 选 adapter 创 device 时会更值得参考

## RAII 智能指针

- UE 用 `TRefCountPtr<T>`（`Engine/Source/Runtime/Core/Public/Templates/RefCounting.h`）
  - `GetInitReference()` 返回 `T**`——拿可写入槽位
  - 类似 `ComPtr<T>::operator&()` 或 `ReleaseAndGetAddressOf()`
- WNanite 用 `Microsoft::WRL::ComPtr`——语义等价

## 设计哲学差异

| 点 | UE D3D12RHI | WNanite |
|---|---|---|
| 兼容性 | Factory4/6 双路径 + 老硬件 | 只 Factory6 |
| 抽象 | `FD3D12Adapter` 类包装 adapter | 内联 main.cpp，无类 |
| 全局状态 | `FD3D12DynamicRHI` 单例 | 无 |
| 多 adapter | 显式选择 + UI / 命令行 | 隐式选 `[0]`（步 05 才用） |
| 同 GPU 多 adapter 去重 | LUID 比对 | 不去重（学习项目可见完整列表） |

**注意**：本仓库不复制 UE5 源码（License）。所有 file:line 仅作对照参考。
