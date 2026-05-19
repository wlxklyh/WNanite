# Step 04 — DXGI Factory + Adapter Enumeration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** WNanite 第一次接 DX12——创 `IDXGIFactory6`、用 `EnumAdapterByGpuPreference` 列出所有显卡（description / vendor / 显存 / LUID / SOFTWARE flag）。仍不创 `ID3D12Device`，不开 Debug Layer。

**Architecture:** 全部内联 `src/main.cpp`：`<dxgi1_6.h>` + `<wrl/client.h>` + `ComPtr` + `IID_PPV_ARGS`。新增 `vendor_name(UINT)` 与 `log_dxgi_adapters()` 两个匿名 namespace 函数；`main` 在 `glfwInit` 之后、`glfwCreateWindow` 之前插一行 `log_dxgi_adapters()`。CMake 加 `dxgi` 链接。

**Tech Stack:** DXGI 1.6 / Windows.h / Microsoft::WRL::ComPtr / C++20。

**对应 Spec：** `Docs/superpowers/specs/2026-05-20-step-04-dxgi-enum-design.md`

---

## File Structure

```
WNanite/
  CMakeLists.txt                         # 修改 — link dxgi
  src/main.cpp                           # 修改 — DX12 第一次出场
  CLAUDE.md                              # 修改 — §10 勾选步 04
  Docs/
    04-dxgi-enum/                        # 新建目录
      README.md                          # 七节学习笔记（含 8 个深度话题）
      ue5-refs.md                        # 引 UE WindowsD3D12Device.cpp
      screenshots/.gitkeep
    superpowers/
      plans/
        2026-05-20-step-04-dxgi-enum.md  # 本文件
```

---

## Task 1: 改 `CMakeLists.txt` 加 `dxgi` 链接

**Files:**
- Modify: `E:\LYH\WNanite\CMakeLists.txt`

- [ ] **Step 1.1: 在 `target_link_libraries` 行加 `dxgi`**

Edit `E:\LYH\WNanite\CMakeLists.txt`:
- old_string: `target_link_libraries(wnanite PRIVATE glfw)`
- new_string: `target_link_libraries(wnanite PRIVATE glfw dxgi)`

- [ ] **Step 1.2: 校验**

Run:
```powershell
Select-String -Path E:\LYH\WNanite\CMakeLists.txt -Pattern "PRIVATE glfw dxgi"
```
Expected: 命中一行（line 58）。

---

## Task 2: 完整替换 `src/main.cpp`

**Files:**
- Modify: `E:\LYH\WNanite\src\main.cpp`（完整覆盖）

- [ ] **Step 2.1: 写新版 main.cpp**

Write `E:\LYH\WNanite\src\main.cpp` (覆盖) with exactly:
```cpp
// WNanite — 主入口
// 当前阶段：步 04 — DXGI Factory + Adapter 枚举。
// 第一次接 DX12；仍不创 D3D12 device（步 05），不开 Debug Layer（步 06）。

#include <GLFW/glfw3.h>

// dxgi1_6.h 拖了一堆 Windows 头，cstdio 等放后面避免 macro 冲突
#include <wrl/client.h>
#include <dxgi1_6.h>

#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace
{
    void on_glfw_error(int code, const char* description)
    {
        std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
    }

    void on_glfw_key(GLFWwindow* window, int key, int /*scancode*/,
                     int action, int /*mods*/)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }

    // Vendor ID → 人类可读厂商名。
    // 参考 https://pcisig.com/membership/member-companies
    const char* vendor_name(UINT vendor_id)
    {
        switch (vendor_id)
        {
            case 0x10DE: return "NVIDIA";
            case 0x1002: return "AMD";
            case 0x1022: return "AMD";        // 早期 AMD ID
            case 0x8086: return "Intel";
            case 0x1414: return "Microsoft/WARP";
            default:     return "Unknown";
        }
    }

    // 枚举所有 DXGI 适配器、打印关键信息到 stdout。
    // 不保存任何 adapter 引用——本步只探测。步 05 才会选定 adapter 创 device。
    bool log_dxgi_adapters()
    {
        ComPtr<IDXGIFactory6> factory;
        HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
        if (FAILED(hr))
        {
            std::fprintf(stderr,
                "CreateDXGIFactory2 failed (hr=0x%08lX). "
                "Likely DXGI < 1.6 (need Win10 1803+).\n",
                static_cast<unsigned long>(hr));
            return false;
        }

        std::printf("DXGI adapters (by HIGH_PERFORMANCE):\n");
        for (UINT i = 0;; ++i)
        {
            ComPtr<IDXGIAdapter1> adapter;
            hr = factory->EnumAdapterByGpuPreference(
                i,
                DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
                IID_PPV_ARGS(&adapter));
            if (hr == DXGI_ERROR_NOT_FOUND) break;
            if (FAILED(hr))
            {
                std::fprintf(stderr,
                    "EnumAdapterByGpuPreference(%u) failed (hr=0x%08lX)\n",
                    i, static_cast<unsigned long>(hr));
                return false;
            }

            DXGI_ADAPTER_DESC1 desc{};
            adapter->GetDesc1(&desc);

            // VRAM / shared 单位转 MiB（1024*1024）；标签写 MB 是显卡社区惯例
            const auto mib = [](SIZE_T bytes) {
                return static_cast<unsigned long long>(bytes) / (1024ull * 1024ull);
            };

            // %S：MSVC 在 char-mode printf 里读 WCHAR* 字符串。
            // 因为 /utf-8 已开 + 控制台 chcp 65001 时显示正确。
            std::printf("[%u] %S\n", i, desc.Description);
            std::printf("    Vendor: %s (0x%04X)  Device: 0x%04X\n",
                vendor_name(desc.VendorId), desc.VendorId, desc.DeviceId);
            std::printf("    VRAM: %llu MB  Shared: %llu MB\n",
                mib(desc.DedicatedVideoMemory),
                mib(desc.SharedSystemMemory));
            std::printf("    LUID: 0x%08lX:0x%08lX\n",
                static_cast<unsigned long>(desc.AdapterLuid.HighPart),
                static_cast<unsigned long>(desc.AdapterLuid.LowPart));

            const bool is_software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            std::printf("    Flags: 0x%08X%s\n",
                desc.Flags,
                is_software ? " [SOFTWARE]" : "");
        }
        return true;
    }
}

int main()
{
    std::printf("hello, WNanite!\n");

    glfwSetErrorCallback(on_glfw_error);
    if (!glfwInit())
    {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    std::printf("GLFW %s\n", glfwGetVersionString());

    // === DX12 第一次出场：DXGI adapter 枚举 ===
    if (!log_dxgi_adapters())
    {
        glfwTerminate();
        return 1;
    }

    // === 窗口（沿用步 03） ===
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720,
                                          "WNanite — step 04",
                                          nullptr, nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwSetKeyCallback(window, on_glfw_key);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
```

- [ ] **Step 2.2: 校验关键字**

Run:
```powershell
$c = Get-Content E:\LYH\WNanite\src\main.cpp -Raw
@(
  '#include <wrl/client.h>',
  '#include <dxgi1_6.h>',
  'using Microsoft::WRL::ComPtr',
  'ComPtr<IDXGIFactory6>',
  'CreateDXGIFactory2(0, IID_PPV_ARGS',
  'EnumAdapterByGpuPreference',
  'DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE',
  'DXGI_ADAPTER_DESC1',
  'DXGI_ADAPTER_FLAG_SOFTWARE',
  'vendor_name',
  'log_dxgi_adapters'
) | ForEach-Object { "$($c.Contains($_))`t$_" }
```
Expected: 11 个 `True` 行。

---

## Task 3: 构建 Debug + Release

> **关键预期**：`main.cpp` 在 `/W4 /permissive-` 下零警告。
> `static_cast<unsigned long>(hr)` / `static_cast<unsigned long long>(...)` 是为消 C4477（printf 长度修饰符不匹配）和 C4244（窄化）。

- [ ] **Step 3.1: 构建 Debug**

Run:
```powershell
cmake --build E:\LYH\WNanite\build-debug --config Debug -j
```
Expected: 末尾 `wnanite.vcxproj -> E:\LYH\WNanite\build-debug\Debug\wnanite.exe`，main.cpp 编译过程**无 warning**，退出码 0。

可能首次 link 出现 `dxgi.lib` 未找到错误——本步先 reconfigure 一次确保新 CMake 设置生效：
```powershell
cmake --preset debug
```
（如果上一次 configure 就 OK 了，重跑也是幂等的）

- [ ] **Step 3.2: 构建 Release**

Run:
```powershell
cmake --preset release; cmake --build E:\LYH\WNanite\build-release --config Release -j
```
Expected: 同 3.1。

---

## Task 4: 交互验证（人工 — 看 stdout + 关窗口）

> 本步要同时验证两件事：
> 1. stdout 含 `DXGI adapters` 区块 + 至少 1 个 `[N]` adapter
> 2. 窗口仍可开、可关
>
> 用 `&` 前缀让 stdout 流回到当前 PowerShell；用户关窗口后命令返回。

- [ ] **Step 4.1: 跑 Debug 二进制**

Run:
```powershell
"=== Debug ==="
& E:\LYH\WNanite\build-debug\Debug\wnanite.exe
"EXIT:$LASTEXITCODE"
```
Expected 输出（你机器上的具体 adapter 不同，但格式应一致）：
```
=== Debug ===
hello, WNanite!
GLFW 3.4.0 Win32 WGL Null EGL OSMesa VisualC
DXGI adapters (by HIGH_PERFORMANCE):
[0] <你的物理显卡描述>
    Vendor: <NVIDIA/AMD/Intel> (0x<id>)  Device: 0x<id>
    VRAM: <非0> MB  Shared: <数千> MB
    LUID: 0x<...>:0x<...>
    Flags: 0x00000000
[<N>] Microsoft Basic Render Driver
    Vendor: Microsoft/WARP (0x1414)  Device: 0x008C
    ...
    Flags: 0x00000002 [SOFTWARE]
<窗口启动 → 请关掉它>
EXIT:0
```

- [ ] **Step 4.2: 跑 Release 二进制**

Run:
```powershell
"=== Release ==="
& E:\LYH\WNanite\build-release\Release\wnanite.exe
"EXIT:$LASTEXITCODE"
```
Expected: stdout 内容与 Debug 一致 + `EXIT:0`。

---

## Task 5: 学习文档 — `Docs/04-dxgi-enum/`

**Files:**
- Create: `Docs/04-dxgi-enum/README.md`
- Create: `Docs/04-dxgi-enum/ue5-refs.md`
- Create: `Docs/04-dxgi-enum/screenshots/.gitkeep`

- [ ] **Step 5.1: 创建目录**

Run:
```powershell
New-Item -ItemType Directory -Force -Path E:\LYH\WNanite\Docs\04-dxgi-enum\screenshots | Out-Null
```

- [ ] **Step 5.2: 写 `screenshots/.gitkeep`（空文件占位）**

Write `E:\LYH\WNanite\Docs\04-dxgi-enum\screenshots\.gitkeep` with empty content.

- [ ] **Step 5.3: 写 `README.md`（七节模板，含 8 个深度学习点）**

Write `E:\LYH\WNanite\Docs\04-dxgi-enum\README.md` with exactly:
````markdown
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

预期 stdout（本机示例，你机器实际不同）：

```
hello, WNanite!
GLFW 3.4.0 Win32 WGL Null EGL OSMesa VisualC
DXGI adapters (by HIGH_PERFORMANCE):
[0] NVIDIA GeForce RTX 4080
    Vendor: NVIDIA (0x10DE)  Device: 0x2782
    VRAM: 16376 MB  Shared: 32687 MB
    LUID: 0x00000000:0x0001AB23
    Flags: 0x00000000
[1] Intel(R) UHD Graphics
    Vendor: Intel (0x8086)  Device: 0x4680
    VRAM: 128 MB  Shared: 32687 MB
    LUID: 0x00000000:0x0001AB42
    Flags: 0x00000000
[2] Microsoft Basic Render Driver
    Vendor: Microsoft/WARP (0x1414)  Device: 0x008C
    VRAM: 0 MB  Shared: 32687 MB
    LUID: 0x00000000:0x0001AB55
    Flags: 0x00000002 [SOFTWARE]
```

实机截图（人工抓 + 落 `screenshots/` 后补）。

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
| `DedicatedVideoMemory` | 真 VRAM (16 GB on RTX 4080) |
| `DedicatedSystemMemory` | **几乎总是 0**，Windows 现代驱动不用这字段 |
| `SharedSystemMemory` | GPU 可访问的系统 RAM (通常 = 总 RAM ÷ 2) |
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

- **`<dxgi1_6.h>` 拉一堆 Windows 头污染 macro**：建议放 `<cstdio>` 之前避免冲突；未来真大量碰到时考虑 `#define WIN32_LEAN_AND_MEAN` 与 `#define NOMINMAX`
- **`printf` 的 `%I64u` vs `%llu`**：MSVC 现代版本两个都认；学习项目用标准 `%llu`，搭配 `static_cast<unsigned long long>(...)` 跨平台更稳
- **`ComPtr<T>::operator&()` 与 `.GetAddressOf()`** 的区别：前者会先 Release 旧引用、后者不会。一般场景两者都行，但 `.GetAddressOf()` 不重置时机更清晰。社区两种都有用
- **CMake 链接系统库不用 find_library**：`target_link_libraries(... dxgi)` 直接写名字即可，Windows SDK 自动处理
- **PowerShell 终端中文显示乱码**：`chcp 65001` 临时改控制台代码页；永久解法是装"使用 Unicode UTF-8 提供全球语言支持"系统选项

## 8. 下一步预告

**步 05：`D3D12 Device` 创建 + Feature Level / SM 检测**——选一张 adapter（默认 `[0]`），调 `D3D12CreateDevice` 创 `ID3D12Device`，用 `CheckFeatureSupport` 查 SM 6.6 / Bindless / AtomicInt64 是否支持。若 SM < 6.6 报错退出（Nanite 门槛）。
````

- [ ] **Step 5.4: 写 `ue5-refs.md`**

Write `E:\LYH\WNanite\Docs\04-dxgi-enum\ue5-refs.md` with exactly:
```markdown
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

**注意**：本仓库不复制 UE5 源码（License）。所有 file:line 仅作对照参考。
```

- [ ] **Step 5.5: 校验三个文档**

Run:
```powershell
@(
  "E:\LYH\WNanite\Docs\04-dxgi-enum\README.md",
  "E:\LYH\WNanite\Docs\04-dxgi-enum\ue5-refs.md",
  "E:\LYH\WNanite\Docs\04-dxgi-enum\screenshots\.gitkeep"
) | ForEach-Object { Test-Path $_ }
```
Expected: 三个 `True`。

---

## Task 6: 勾选 CLAUDE.md §10 进度

**Files:**
- Modify: `E:\LYH\WNanite\CLAUDE.md`（§10 Phase A 第 4 项）

- [ ] **Step 6.1: 勾选步 04**

Edit `E:\LYH\WNanite\CLAUDE.md`:
- old_string: `- [ ] 04 DXGI Factory + Adapter 枚举`
- new_string: `- [x] 04 DXGI Factory + Adapter 枚举 — [Docs/04-dxgi-enum](Docs/04-dxgi-enum/README.md)`

- [ ] **Step 6.2: 校验**

Run:
```powershell
Select-String -Path E:\LYH\WNanite\CLAUDE.md -Pattern "\[x\] 04 DXGI"
```
Expected: 命中一行。

---

## Task 7: step-04 commit + push origin

- [ ] **Step 7.1: 暂存所有改动**

Run:
```powershell
git -C E:\LYH\WNanite add CMakeLists.txt src/main.cpp CLAUDE.md Docs/04-dxgi-enum/ Docs/superpowers/specs/2026-05-20-step-04-dxgi-enum-design.md Docs/superpowers/plans/2026-05-20-step-04-dxgi-enum.md
```

- [ ] **Step 7.2: 校验暂存**

Run:
```powershell
git -C E:\LYH\WNanite status --short
```
Expected: 仅 `A` / `M` 条目；无 `build-*` / `_deps/`。

- [ ] **Step 7.3: 提交**

Run:
```powershell
git -C E:\LYH\WNanite commit -m "step-04: DXGI Factory + adapter enumeration"
```
Expected: `[main <sha>] step-04: DXGI Factory + adapter enumeration` + 多 files changed。

- [ ] **Step 7.4: Push**

Run:
```powershell
git -C E:\LYH\WNanite push origin main
```
Expected: `<old-sha>..<new-sha>  main -> main` fast-forward。

- [ ] **Step 7.5: 校验**

Run:
```powershell
git -C E:\LYH\WNanite log --oneline --decorate -n 5; Write-Output "---"; git -C E:\LYH\WNanite status
```
Expected:
```
<sha> (HEAD -> main, origin/main) step-04: DXGI Factory + adapter enumeration
9526834 docs(CLAUDE): add UE5 source reference rule
920a54f step-03: GLFW window lifecycle + event loop
34aa671 step-02: introduce GLFW via FetchContent
03fc58f step-01: CMake hello-world
---
On branch main
Your branch is up to date with 'origin/main'.

nothing to commit, working tree clean
```

---

## Done 校验（对照 CLAUDE.md §3 单步完成定义）

- [ ] 1. Debug + Release 编译通过 — Task 3
- [ ] 2. 运行通过 — Task 4 × 2
- [ ] 3. 验收信号 — `DXGI adapters` 区块 + ≥1 个 adapter + 窗口可关 + exit 0
- [ ] 4. `Docs/04-dxgi-enum/README.md` 七节 + 8 个深度学习点 — Task 5
- [ ] 5. ai-learn 不适用（API 流程类） — N/A
- [ ] 6. doctest 不适用（无纯逻辑） — N/A
- [ ] 7. golden PNG 不适用 — N/A
- [ ] 8. CLAUDE.md §10 已勾 — Task 6
- [ ] Bonus: 已 push origin — Task 7.4

全部 ✓ 后步 04 完成，可进入步 05 brainstorming（`D3D12Device` 创建 + Feature Level 检测）。
