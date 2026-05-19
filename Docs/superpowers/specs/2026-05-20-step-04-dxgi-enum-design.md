# 步 04 — DXGI Factory + Adapter 枚举 + 日志 设计文档

**日期**：2026-05-20
**作者**：linyanhou98@gmail.com + Claude
**状态**：草案（待用户审阅）
**附属于**：`Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md` §5 Phase A 步 04

---

## 1. 目标

WNanite 第一次接触 DX12：

- 创建 `IDXGIFactory6`
- 用 `EnumAdapterByGpuPreference` 按"高性能优先"枚举所有显卡
- 每个适配器打印：description / vendor name / vendor id / device id / 显存 / shared memory / LUID / flags（含 software）
- **仍不创 `ID3D12Device`**（步 05 才做）
- 不开 DXGI / D3D12 Debug Layer（步 06 一并开）

走完本步：`wnanite.exe` 启动后 stdout 多出 `DXGI adapters` 区块；窗口仍可开/可关。

## 2. 已锁定的技术决策

| 维度 | 决定 | 理由 |
|---|---|---|
| COM 引用计数 | `Microsoft::WRL::ComPtr` | UE D3D12RHI 用 `TRefCountPtr`（等价 RAII）109 处分布 20 文件；ComPtr 是 D3D12 社区事实标准 |
| Factory 版本 | `IDXGIFactory6` 经 `CreateDXGIFactory2(0, IID_PPV_ARGS(&factory))` | 提供 `EnumAdapterByGpuPreference`；Win10 1803+ 通行 |
| 枚举 API | `EnumAdapterByGpuPreference(i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, ...)` | 多显卡机器（笔记本独显+核显）独显排第一 |
| Adapter 接口 | `IDXGIAdapter1` + `GetDesc1` → `DXGI_ADAPTER_DESC1` | DESC1 含 `Flags`（识别 `DXGI_ADAPTER_FLAG_SOFTWARE` WARP） |
| DXGI Debug Layer | **不开** | 留给步 06 与 D3D12 Debug Layer 一并开 |
| 错误处理 | 直接 `if (FAILED(hr))` + `std::fprintf stderr` + 优雅退出 | 不引宏；学习项目每个 check 都看见 |
| 打印目标 | stdout | spdlog 步 12 才接 |
| 打印时机 | `glfwInit` 后、`glfwCreateWindow` 前 | 启动先看硬件信息，崩在 DXGI 也能看清；窗口开后再 logs CPU 占用就高了 |
| 代码组织 | 内联 `main.cpp` | YAGNI；步 ≥ 04 累积到 ~3 个职责再考虑拆 |
| CMake 链接 | `target_link_libraries(wnanite PRIVATE glfw dxgi)` | 需 `dxgi.lib` 拿 `CreateDXGIFactory2` |

## 3. 文件改动清单

| 文件 | 动作 | 关键变化 |
|------|------|---------|
| `CMakeLists.txt` | 修改 | `target_link_libraries(... PRIVATE glfw dxgi)` |
| `src/main.cpp` | 修改 | 加 `<dxgi1_6.h>` / `<wrl/client.h>` / vendor_name helper / log_dxgi_adapters / main 内插入调用 |
| `Docs/04-dxgi-enum/` | 新建目录 | `README.md` 七节 + `ue5-refs.md`（引 UE WindowsD3D12Device.cpp 适配点）+ `screenshots/.gitkeep` |
| `CLAUDE.md` | 修改 | §10 Phase A 步 04 由 `[ ]` 改 `[x]` |

## 4. CMakeLists.txt diff

把当前的：
```cmake
target_link_libraries(wnanite PRIVATE glfw)
```
改成：
```cmake
target_link_libraries(wnanite PRIVATE glfw dxgi)
```

注意：**不**链接 `d3d12.lib` 与 `dxguid.lib`：
- `d3d12.lib` 步 05 才需（`D3D12CreateDevice`）
- `dxguid.lib` 现代 SDK 已不需要，IID 由 `IID_PPV_ARGS` 宏 + `__uuidof` 编译期解析

## 5. src/main.cpp 完整新版本

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

            // VRAM / shared 单位转 MB（10^6 还是 2^20？这里用 MiB = 1024*1024）
            const auto mib = [](SIZE_T bytes) {
                return static_cast<unsigned long long>(bytes) / (1024ull * 1024ull);
            };

            // %S：MSVC 在 char-mode printf 里读 WCHAR* 字符串（与 GNU 相反）。
            // 因为 /utf-8 已开，控制台代码页是 65001 时显示正确。
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

## 6. 预期 stdout（在本机示例，具体显卡按你机器变）

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

至少出现 1 张物理显卡 + 1 张 WARP。其余字段（VRAM / Device / LUID）按机器变，不强行比对。

## 7. 验证

- **编译**：Debug + Release 双 preset 通过；`src/main.cpp` 在 `/W4 /permissive-` 下零警告
  - 注意 `static_cast<unsigned long>(hr)`、`static_cast<unsigned long long>(...)` 是为了 `printf` 长度修饰符匹配，消 C4477
- **运行**：交互启动 → stdout 含 `DXGI adapters` 区块且 ≥ 1 个 `[N]` 条目 → 窗口仍可开 → 关闭按钮 / ESC 退出 → 退出码 0
- **回归**：步 02 + 03 的输出 / 行为不变
- 不写单测（DXGI API 调用层，按 CLAUDE.md §7）
- 无 golden image（窗口由 OS 渲）
- 无 ai-learn（API 流程类）

## 8. UE5 对照（与新 §1 规则呼应）

UE D3D12RHI 的 adapter 枚举入口：

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Device.cpp:813-863`
  - 同时拿 `TRefCountPtr<IDXGIFactory4>` 和 `TRefCountPtr<IDXGIFactory6>`
  - 用 `FD3D12AdapterDesc::EnumAdapters(...)` 包装，内部分支 Factory6 走 `EnumAdapterByGpuPreference`，老 Factory 走 `EnumAdapters`
  - 把 `TempAdapter.GetInitReference()` 传给枚举 API 的 `void**` 输出
- 我们简化掉 Factory4 fallback——学习项目门槛卡 Win10 1803+ 没问题

`ue5-refs.md` 会把这些 file:line 全部记录。

## 9. 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| 老系统没 `IDXGIFactory6` | `CreateDXGIFactory2` 返回 E_NOINTERFACE 但 IID 写法 + `IID_PPV_ARGS` 处理后会拿 nullptr | 错误打印 + return 1。学习项目门槛接受 |
| `desc.Description` 是 `WCHAR*` 含中文 / 特殊符号 | char-mode `printf` 用 `%S` 在 MSVC 控制台代码页非 UTF-8 时可能乱码 | `/utf-8` 已开；终端 chcp 65001 是常态 |
| 多显卡机器某些 LUID 重复 | LUID 理论上唯一；如果重复说明驱动状态异常 | 本步打印用于人工查看，不做唯一性 assert |
| 没显卡（VM 无 GPU 直通） | 至少有 WARP 适配器 | WARP 也会出现在列表里，验收不卡 |

## 10. 范围外（明确）

- 不创 `ID3D12Device`（步 05）
- 不调 `D3D12CreateDevice` 探测 Feature Level（步 05）
- 不开 DXGI / D3D12 Debug Layer（步 06）
- 不存"chosen adapter"全局变量（步 05 才存）
- 不写多显卡选择 UI（imgui 步 30 之后）
- 不打印 Outputs / monitor 信息（暂无需要）

## 11. 下一步流程

1. 用户审阅
2. 调 `superpowers:writing-plans` 出实施计划
3. inline 执行 + verification + Docs + step-04 commit + push origin
