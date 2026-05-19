# 步 04 — DXGI Factory + Adapter 枚举

> Phase A · Bootstrap · 第 4 步 / 共 11 步
> 对应 Spec：`Docs/superpowers/specs/2026-05-20-step-04-dxgi-enum-design.md`
> **第一次接 DX12** —— 但只用 DXGI 探测，不创 device。

---

## 1. 本步学了什么

WNanite **第一次和 DX12 生态打照面**——但只做最浅一层：

- `CreateDXGIFactory2` → `IDXGIFactory6`
- `EnumAdapterByGpuPreference(... HIGH_PERFORMANCE ...)` 按"高性能优先"枚举
- `IDXGIAdapter1::GetDesc1` 拿 `DXGI_ADAPTER_DESC1`
- 用 `Microsoft::WRL::ComPtr` 自动管 COM 引用计数
- `IID_PPV_ARGS` 宏 + HRESULT 错误模型 + `DXGI_ERROR_NOT_FOUND` 哨兵

走完本步：`wnanite.exe` 启动后 stdout 多出 `DXGI adapters` 区块——列出你机器上所有显卡 + WARP 软件适配器。窗口仍可开/可关。

## 2. 为什么这么做

**为什么先 DXGI 不 D3D12**：

DXGI 与 D3D12 是两层：
- **DXGI** 是"系统视角"——adapters / outputs / swap chains / display modes，由 OS+驱动暴露
- **D3D12** 是"设备视角"——device / command list / resources / PSO，要先选 adapter 再创 device

步 04 用 DXGI 探测**不创 device**，把"硬件能力调研"与"实际 device 创建"两件事分开——架构上 DXGI 本来就独立，先用它探明所有显卡，步 05 再选一张创 device。

**为什么 `IDXGIFactory6` + `EnumAdapterByGpuPreference`**：

DXGI 1.6 之前的 `EnumAdapters1` 顺序由驱动决定——**笔记本独显+核显场景经常默认枚到核显**。1.6 加 `EnumAdapterByGpuPreference` 才能稳定按性能排。

UE D3D12RHI 同时拿 Factory4 + Factory6 兼容降级；我们简化掉降级，门槛卡 Win10 1803+ 没问题。

**为什么 `ComPtr` 不裸指针**：

UE D3D12RHI 用 `TRefCountPtr<ID3D12*>` 109 处分布 20 文件——RAII 管引用计数是行业标准。`ComPtr` 与 `TRefCountPtr` 语义等价。

裸指针 + 手动 `Release()` 在异常路径 / early return 容易漏，本项目这种 50+ pass 的渲染框架不能忍。

**为什么不开 DXGI Debug Layer**：

步 06 与 D3D12 Debug Layer 一并开（节奏统一）。步 04 先用最干净的探测路径，遇到问题再加 debug 层。

**为什么打印目标是 stdout 不 spdlog**：

spdlog 步 12 才接。步 04-11 暂用 stdout/stderr，步 12 一次性把所有打印重定向到 spdlog 双 sink。

## 3. 代码导读

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `src/main.cpp:8-9` | `<wrl/client.h>` / `<dxgi1_6.h>` | DX12 头依赖；顺序在 `<cstdio>` 之前避免 macro 污染 |
| `src/main.cpp:13` | `using Microsoft::WRL::ComPtr` | 全文件用 ComPtr |
| `src/main.cpp:33-44` | `vendor_name` | PCI Vendor ID → 厂商名（5 种） |
| `src/main.cpp:48-104` | `log_dxgi_adapters` 整个函数 | DXGI 探测主体 |
| `src/main.cpp:50-59` | `CreateDXGIFactory2 + IID_PPV_ARGS` | 创建 Factory6（含失败分支） |
| `src/main.cpp:62-102` | 主 `for` 循环 | `EnumAdapterByGpuPreference` + `DXGI_ERROR_NOT_FOUND` 哨兵 |
| `src/main.cpp:82-84` | `mib` lambda | 字节→MiB 转换 |
| `src/main.cpp:88-101` | 五行 printf | description / vendor / VRAM / LUID / flags |
| `src/main.cpp:120-124` | `if (!log_dxgi_adapters())` | main 在 init/version 之后、开窗之前调 |

## 4. UE5 是怎么做的

UE 的 adapter 枚举入口：

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Device.cpp:813-863`
  - 同时拿 `TRefCountPtr<IDXGIFactory4>` 与 `TRefCountPtr<IDXGIFactory6>`
  - 用 `FD3D12AdapterDesc::EnumAdapters` 包装兼容降级
  - 把 `TempAdapter.GetInitReference()` 传给枚举 API 的 `void**`

`TRefCountPtr::GetInitReference()` 等价于 `ComPtr::operator&()`——拿可写入的 `void**` 输出槽位。

详见 `ue5-refs.md`。

## 5. 截图 / GIF

实机 stdout（本机：NVIDIA RTX 5060 + AMD Radeon 核显的笔记本）：

```
hello, WNanite!
GLFW 3.4.0 Win32 WGL Null EGL OSMesa VisualC
DXGI adapters (by HIGH_PERFORMANCE):
[0] NVIDIA GeForce RTX 5060
    Vendor: NVIDIA (0x10DE)  Device: 0x2D05
    VRAM: 7895 MB  Shared: 31516 MB
    LUID: 0x00000000:0x00012AC8
    Flags: 0x00000000
[1] AMD Radeon(TM) Graphics
    Vendor: AMD (0x1002)  Device: 0x13C0
    VRAM: 2021 MB  Shared: 31516 MB
    LUID: 0x00000000:0x00013E40
    Flags: 0x00000000
[2] Microsoft Basic Render Driver
    Vendor: Microsoft/WARP (0x1414)  Device: 0x008C
    VRAM: 0 MB  Shared: 31516 MB
    LUID: 0x00000000:0x00013E0D
    Flags: 0x00000002 [SOFTWARE]
[3] NVIDIA GeForce RTX 5060
    Vendor: NVIDIA (0x10DE)  Device: 0x2D05
    VRAM: 7895 MB  Shared: 31516 MB
    LUID: 0x00000000:0x000809EE
    Flags: 0x00000000
[4] NVIDIA GeForce RTX 5060
    Vendor: NVIDIA (0x10DE)  Device: 0x2D05
    VRAM: 7895 MB  Shared: 31516 MB
    LUID: 0x00000000:0x00082210
    Flags: 0x00000000
```

`EnumAdapterByGpuPreference(HIGH_PERFORMANCE)` 把 RTX 5060 排到 `[0]` —— 不会枚到 AMD 核显去（如果机器跑双显，旧 `EnumAdapters1` 默认会枚错）。

## 6. 8 个深度学习点

这是步 04 真正值钱的内容——表面平淡，里面密度极高：

### 6.1 ComPtr / IID_PPV_ARGS — DX12 代码的基础语法

```cpp
ComPtr<IDXGIFactory6> factory;
CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
```

- `IID_PPV_ARGS(&factory)` 宏展开成 `__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&factory)`——一举两得（IID + void\*\*）
- `ComPtr<T>::operator&()` 不是普通取地址——先 `Release()` 旧引用、再返回 `T**`。RAII 不漏 release 的关键
- **整个 DX12 代码都用这套语法**

### 6.2 DXGI vs D3D12 的职责分割

| 层 | 管什么 | 谁拥有 |
|---|---|---|
| DXGI | 系统/硬件：adapters / outputs / swap chains | OS+驱动 |
| D3D12 | 设备：device / cmdlist / resources / PSO | 你创建的 device |

DXGI 不需要 device 就能存在。后面 swap chain（DXGI 创但服务 D3D12 device）的"跨层"API 看起来奇怪，理解了这个分层就合理。

### 6.3 `EnumAdapterByGpuPreference` 与多 GPU 的历史

DXGI 1.6 之前 `EnumAdapters1` 顺序由驱动决定，笔记本独显+核显场景常默认枚到核显。1.6 之后才能稳定按性能排。

UE 的处理（`WindowsD3D12Device.cpp:813-863`）：同时拿 Factory4 + Factory6，新 Factory6 走 `EnumAdapterByGpuPreference`，老 Factory4 走 `EnumAdapters`——**渐进降级**。学习项目可以简化。

### 6.4 `DXGI_ADAPTER_DESC1` 字段实际含义

| 字段 | 你预期的 vs 真相 |
|---|---|
| `DedicatedVideoMemory` | 真 VRAM (RTX 5060 = 7895 MB) |
| `DedicatedSystemMemory` | **几乎总是 0**，Windows 现代驱动不用这字段 |
| `SharedSystemMemory` | GPU 可访问的系统 RAM (通常 = 总 RAM ÷ 2，本机 31516 MB) |
| `AdapterLuid` | 64-bit ID，跨进程稳定但重启/驱动重装会变 |
| `Flags` | `DXGI_ADAPTER_FLAG_SOFTWARE` bit 区分 WARP |

**`DedicatedSystemMemory` 永远是 0** 是个典型的接口历史包袱。

### 6.5 HRESULT 与 `DXGI_ERROR_NOT_FOUND` 的循环退出

```cpp
for (UINT i = 0;; ++i) {
    hr = factory->EnumAdapterByGpuPreference(i, ..., &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) break;     // 不是错误，是"枚完了"
    if (FAILED(hr)) return false;              // 真错
}
```

`DXGI_ERROR_NOT_FOUND` 是 COM 风格的**友好失败哨兵**——`SUCCEEDED`/`FAILED` 宏不够区分"枚到底"和"真错"。

HRESULT 内部 32-bit packed（severity + facility + code）。打印 `0x%08lX` 查官方错误对照表。

### 6.6 WARP 是什么 / 为什么列表里有

`Microsoft Basic Render Driver` = WARP（Windows Advanced Rasterization Platform）= **纯 CPU 实现的 D3D12 驱动**。

- 跑得动 D3D11/D3D12 大多数 API（含基础 Compute）
- **没硬件能力**：无 BindlessResourceDescriptor / 无 AtomicInt64 / 极慢
- 用途：CI、服务器、无 GPU VM、调试驱动行为
- **Nanite 完全跑不动 WARP**——这是后续"必须真 GPU"的判别

### 6.7 `printf("%S", desc.Description)` 的小坑

`DXGI_ADAPTER_DESC1::Description` 是 `WCHAR[128]`（UTF-16 LE）。`printf` 里：

- **MSVC**：`%S` = WCHAR\*，`%s` = char\*
- **glibc/clang POSIX**：**反过来**

跨平台不能直接 `%S`。MSVC + `/utf-8` + 控制台 `chcp 65001` 三个条件凑齐，中文/特殊字符才不乱码。

### 6.8 与 UE 对照 — 工业代码的"渐进降级"

```cpp
// UE: WindowsD3D12Device.cpp:838
TRefCountPtr<IDXGIAdapter> TempAdapter;
for (uint32 i = 0;
     FD3D12AdapterDesc::EnumAdapters(i, GpuPreference,
         DXGIFactory4, DXGIFactory6, TempAdapter.GetInitReference())
         != DXGI_ERROR_NOT_FOUND;
     ++i)
{ ... }
```

对比 WNanite：

```cpp
ComPtr<IDXGIAdapter1> adapter;
for (UINT i = 0;
     factory->EnumAdapterByGpuPreference(i, ..., IID_PPV_ARGS(&adapter))
         != DXGI_ERROR_NOT_FOUND;
     ++i)
{ ... }
```

骨架一致。差别：UE 多一层 `FD3D12AdapterDesc::EnumAdapters` 包装 Factory4/6 双路径。**学习项目的"理解 > 模仿"**——简化降级路径是合理的，等真要支持老硬件时再加。

## 7. 踩过的坑

### 7.1 同一张物理 GPU 会被枚成多个 adapter（实测）

本机 NVIDIA RTX 5060 在 DXGI 列表里出现 **3 次**（不同 LUID，相同 description / vendor / device / VRAM）：

```
[0] NVIDIA GeForce RTX 5060  LUID: 0x00012AC8  ← 主独显 adapter
[3] NVIDIA GeForce RTX 5060  LUID: 0x000809EE  ← 同卡，不同 LUID
[4] NVIDIA GeForce RTX 5060  LUID: 0x00082210  ← 同卡，又一个 LUID
```

这是 **NVIDIA 驱动给同一物理 GPU 暴露多个 DXGI adapter** 的产物。可能的原因：

- **Multi-GPU linked-node**：D3D12 允许"一张物理 GPU 内多 node"，每个 node 是独立 adapter
- **Studio/Game-Ready 驱动**：DCH 驱动为 OptiX / CUDA / DX 各自暴露独立 adapter
- **Hybrid GPU 路径**：与 Windows OS 的混合渲染（Optimus 续作）有关

**实战教训**：
- 不能只看"adapter 数量"来推断"机器上有几张 GPU"
- 选 adapter 时只看 description 不够（会有 3 个同名），**必须靠 LUID + Flags + GpuPreference 排序**联合判断
- UE 用 LUID 去重（`D3D12Adapter.cpp` 里有相关逻辑）

这是文档不会告诉你的——只有真跑过 `EnumAdapterByGpuPreference` 看实机输出才会撞上。

### 7.2 `<dxgi1_6.h>` 拉一堆 Windows 头污染 macro

`min`/`max`/`CreateWindow` 等 Win32 macro 会污染。
- 建议放 `<cstdio>` / 标准库头之前避免冲突
- 未来真大量碰到时考虑 `#define WIN32_LEAN_AND_MEAN` 与 `#define NOMINMAX`

### 7.3 `printf` 的 `%I64u` vs `%llu`

MSVC 现代版本两个都认；学习项目用标准 `%llu`，搭配 `static_cast<unsigned long long>(...)` 跨平台更稳。

### 7.4 `ComPtr<T>::operator&()` 与 `.GetAddressOf()` 的区别

- `operator&()`：**先 Release 旧引用，再返回 T\*\***
- `.GetAddressOf()`：**直接返回 T\*\***，不重置旧引用
- 二者大多数场景行为相同，但 `.GetAddressOf()` 时机更清晰；社区两种都常见

### 7.5 CMake 链接 Windows 系统库不用 `find_library`

`target_link_libraries(... dxgi)` 直接写名字即可，Windows SDK 自动处理。`d3d12` / `dxguid` / `d3dcompiler` 同理。

### 7.6 PowerShell 终端中文显示乱码

`chcp 65001` 临时改控制台代码页；永久解法是装"使用 Unicode UTF-8 提供全球语言支持"系统选项。

### 7.7 PowerShell 重定向 stdout 时子进程全缓冲

直接 `& exe` 让子进程继承 PS 终端时，stdout 是行缓冲；但用 `Start-Process -RedirectStandardOutput` 或后台运行时变全缓冲，进程退出前看不到输出。**本步实测**——开窗口后 stdout 内容没出现在 output file，直到关窗后才一次性 flush。

## 8. 下一步预告

**步 05：`D3D12 Device` 创建 + Feature Level / SM 检测**——选一张 adapter（默认 `[0]`），调 `D3D12CreateDevice` 创 `ID3D12Device`，用 `CheckFeatureSupport` 查 SM 6.6 / Bindless / AtomicInt64 是否支持。若 SM < 6.6 报错退出（Nanite 门槛）。
