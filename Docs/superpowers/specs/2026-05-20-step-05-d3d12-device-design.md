# 步 05 — D3D12 Device + Feature Level / SM 检测 设计文档

**日期**：2026-05-20
**作者**：linyanhou98@gmail.com + Claude
**状态**：草案（待用户审阅）
**附属于**：`Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md` §5 Phase A 步 05

---

## 1. 目标

WNanite **第一次创建 `ID3D12Device`**——选一张 adapter、调 `D3D12CreateDevice` 创设备、用 `CheckFeatureSupport` 探测一组能力，验证 **Nanite 硬门槛**：

- Max Shader Model ≥ `D3D_SHADER_MODEL_6_6`
- Resource Binding Tier ≥ 3（Bindless 必需）

任一不达标立即 stderr 报错 + `return 1`。门槛过则继续到窗口循环。

**仍不开 Debug Layer（步 06）**。**仍不创 Command Queue / SwapChain（步 07+）**。

## 2. 已锁定的技术决策

| 维度 | 决定 | 理由 |
|---|---|---|
| 选 adapter 策略 | 在枚举循环里选**第一张非 `DXGI_ADAPTER_FLAG_SOFTWARE`** 的 | 步 04 实测 `[0]` 即 NVIDIA 独显；跳 software 防 WARP 自动选 |
| `D3D12CreateDevice` 的 MinFeatureLevel | `D3D_FEATURE_LEVEL_12_0` | UE `D3D12Adapter.cpp:1350` Nanite 实际门槛 = 12_0（不是 12_2） |
| SM 探测算法 | "从高到低试"——`D3D_SHADER_MODEL_6_9` → 6_8 → ... → 6_0；第一个成功的就是 max | 与 UE `WindowsD3D12Device.cpp:173 FindHighestShaderModel` 一致；`CheckFeatureSupport` 的 `HighestShaderModel` 必须给目标 SM 作输入 hint |
| 顺带探测的能力 | Max Feature Level / SM / Binding Tier / Heap Tier / Wave Ops / AtomicInt64 / Mesh Shader Tier / VRS Tier | 学习项目顺手打能力报告；后续步要用时不用再查 |
| Nanite 硬门槛 | `SM >= 6.6 && BindingTier >= 3` | UE 实证（`D3D12Adapter.cpp:1076, 1350`）；同时 SM 6.6 + Tier 3 才支持 `ResourceDescriptorHeap` |
| 门槛不过 | stderr 红字打印 + `return 1` | 跑不动就早退，浪费时间在不能用的硬件上 |
| Debug Layer | **不开** | 步 06 一并开 D3D12 + DXGI Debug |
| 代码组织 | 内联 main.cpp，新增 5 个匿名 namespace helper：`pick_adapter` / `create_device` / `query_caps` / `log_caps` / `check_nanite_gate` | 学习项目 YAGNI；累积到 ~300 行后再拆模块（预计步 ≥ 08） |
| CMake 链接 | `target_link_libraries(... PRIVATE glfw dxgi d3d12)` | `D3D12CreateDevice` 来自 d3d12.lib |
| Adapter 选择反馈 | 跑步骤打印 `Selected adapter: [<i>] <description>` | 用户一眼看到选了哪张 |

## 3. 文件改动清单

| 文件 | 动作 | 关键变化 |
|------|------|---------|
| `CMakeLists.txt` | 修改 | `target_link_libraries(... PRIVATE glfw dxgi d3d12)` |
| `src/main.cpp` | 大改 | 拆 `log_dxgi_adapters` 为 `pick_adapter`；新增 `create_device` / `query_caps` / `log_caps` / `check_nanite_gate`；main 串起流水线；加 `<d3d12.h>` 头 |
| `Docs/05-d3d12-device/` | 新建目录 | README 七节 + ue5-refs（重点引 UE FindHighestShaderModel + Nanite 门槛常量）+ screenshots/.gitkeep |
| `CLAUDE.md` | 修改 | §10 步 05 由 `[ ]` 改 `[x]` |

## 4. CMakeLists.txt diff

```diff
- target_link_libraries(wnanite PRIVATE glfw dxgi)
+ target_link_libraries(wnanite PRIVATE glfw dxgi d3d12)
```

## 5. src/main.cpp 完整新版本

```cpp
// WNanite — 主入口
// 当前阶段：步 05 — D3D12 Device 创建 + Feature Level / SM 检测。
// 第一次创 ID3D12Device；仍不开 Debug Layer（步 06），不创 Command Queue（步 07）。

#include <GLFW/glfw3.h>

#include <wrl/client.h>
#include <dxgi1_6.h>
#include <d3d12.h>

#include <cstdio>

using Microsoft::WRL::ComPtr;

namespace
{
    // ===== GLFW callbacks =====

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

    // ===== Helpers =====

    const char* vendor_name(UINT vendor_id)
    {
        switch (vendor_id)
        {
            case 0x10DE: return "NVIDIA";
            case 0x1002: return "AMD";
            case 0x1022: return "AMD";
            case 0x8086: return "Intel";
            case 0x1414: return "Microsoft/WARP";
            default:     return "Unknown";
        }
    }

    const char* feature_level_str(D3D_FEATURE_LEVEL fl)
    {
        switch (fl)
        {
            case D3D_FEATURE_LEVEL_12_2: return "12.2";
            case D3D_FEATURE_LEVEL_12_1: return "12.1";
            case D3D_FEATURE_LEVEL_12_0: return "12.0";
            case D3D_FEATURE_LEVEL_11_1: return "11.1";
            case D3D_FEATURE_LEVEL_11_0: return "11.0";
            default:                     return "<unknown>";
        }
    }

    const char* shader_model_str(D3D_SHADER_MODEL sm)
    {
        switch (sm)
        {
            // Windows SDK 10.0.22621 max = 6.7。升级 26100+ 或装 Agility SDK 才能拿到 6_8 / 6_9。
            case D3D_SHADER_MODEL_6_7: return "6.7";
            case D3D_SHADER_MODEL_6_6: return "6.6";
            case D3D_SHADER_MODEL_6_5: return "6.5";
            case D3D_SHADER_MODEL_6_4: return "6.4";
            case D3D_SHADER_MODEL_6_3: return "6.3";
            case D3D_SHADER_MODEL_6_2: return "6.2";
            case D3D_SHADER_MODEL_6_1: return "6.1";
            case D3D_SHADER_MODEL_6_0: return "6.0";
            case D3D_SHADER_MODEL_5_1: return "5.1";
            default:                   return "<unknown>";
        }
    }

    void log_one_adapter(UINT i, const DXGI_ADAPTER_DESC1& desc)
    {
        const auto mib = [](SIZE_T bytes) {
            return static_cast<unsigned long long>(bytes) / (1024ull * 1024ull);
        };
        std::printf("[%u] %S\n", i, desc.Description);
        std::printf("    Vendor: %s (0x%04X)  Device: 0x%04X\n",
            vendor_name(desc.VendorId), desc.VendorId, desc.DeviceId);
        std::printf("    VRAM: %llu MB  Shared: %llu MB\n",
            mib(desc.DedicatedVideoMemory), mib(desc.SharedSystemMemory));
        std::printf("    LUID: 0x%08lX:0x%08lX\n",
            static_cast<unsigned long>(desc.AdapterLuid.HighPart),
            static_cast<unsigned long>(desc.AdapterLuid.LowPart));

        const bool is_software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
        std::printf("    Flags: 0x%08X%s\n",
            desc.Flags, is_software ? " [SOFTWARE]" : "");
    }

    // ===== Step 04 行为 + Step 05 选择 =====

    struct AdapterChoice
    {
        ComPtr<IDXGIAdapter1> adapter;
        DXGI_ADAPTER_DESC1   desc{};
        UINT                 index = UINT_MAX;
    };

    bool pick_adapter(IDXGIFactory6* factory, AdapterChoice& out)
    {
        std::printf("DXGI adapters (by HIGH_PERFORMANCE):\n");
        for (UINT i = 0;; ++i)
        {
            ComPtr<IDXGIAdapter1> adapter;
            HRESULT hr = factory->EnumAdapterByGpuPreference(
                i, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
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
            log_one_adapter(i, desc);

            // 选择策略：第一张非 software 的 adapter。
            const bool is_software = (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) != 0;
            if (!is_software && out.index == UINT_MAX)
            {
                out.adapter = adapter;
                out.desc    = desc;
                out.index   = i;
            }
        }

        if (out.index == UINT_MAX)
        {
            std::fprintf(stderr,
                "No non-software adapter found. Nanite needs a real GPU.\n");
            return false;
        }
        std::printf("Selected adapter: [%u] %S\n", out.index, out.desc.Description);
        return true;
    }

    // ===== D3D12 Device 创建 =====

    ComPtr<ID3D12Device> create_device(IDXGIAdapter1* adapter)
    {
        std::printf("Creating D3D12 device at Feature Level 12.0 ... ");
        ComPtr<ID3D12Device> device;
        HRESULT hr = D3D12CreateDevice(
            adapter,
            D3D_FEATURE_LEVEL_12_0,
            IID_PPV_ARGS(&device));
        if (FAILED(hr))
        {
            std::printf("FAIL\n");
            std::fprintf(stderr,
                "D3D12CreateDevice failed (hr=0x%08lX). "
                "Adapter likely below FL 12.0.\n",
                static_cast<unsigned long>(hr));
            return nullptr;
        }
        std::printf("OK\n");
        return device;
    }

    // ===== Capability 探测 =====

    struct DeviceCaps
    {
        D3D_FEATURE_LEVEL  max_feature_level   = D3D_FEATURE_LEVEL_11_0;
        D3D_SHADER_MODEL   max_shader_model    = D3D_SHADER_MODEL_5_1;
        UINT               binding_tier        = 0;
        UINT               heap_tier           = 0;
        bool               wave_ops            = false;
        UINT               wave_min            = 0;
        UINT               wave_max            = 0;
        bool               atomic_int64_typed  = false;
        UINT               mesh_shader_tier    = 0;
        UINT               vrs_tier            = 0;
    };

    DeviceCaps query_caps(ID3D12Device* device)
    {
        DeviceCaps caps;

        // --- Max Feature Level ---
        // 从 12_2 → 11_0 试，取第一个 supported。
        // D3D12_FEATURE_DATA_FEATURE_LEVELS 接受候选列表，driver 选最高支持的。
        D3D_FEATURE_LEVEL candidates[] = {
            D3D_FEATURE_LEVEL_12_2,
            D3D_FEATURE_LEVEL_12_1,
            D3D_FEATURE_LEVEL_12_0,
            D3D_FEATURE_LEVEL_11_1,
            D3D_FEATURE_LEVEL_11_0,
        };
        D3D12_FEATURE_DATA_FEATURE_LEVELS fls{};
        fls.NumFeatureLevels = _countof(candidates);
        fls.pFeatureLevelsRequested = candidates;
        if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_FEATURE_LEVELS, &fls, sizeof(fls))))
        {
            caps.max_feature_level = fls.MaxSupportedFeatureLevel;
        }

        // --- Max Shader Model ---
        // CheckFeatureSupport 的 HighestShaderModel 字段是 in/out：
        // 输入"我想问 SM x 是否支持"，输出"实际支持到的最高 SM (≤ 输入)"。
        // 从高到低试。SDK 10.0.22621 最高定义到 6.7；硬件可能更高但我们的 SDK 看不见。
        const D3D_SHADER_MODEL sm_candidates[] = {
            D3D_SHADER_MODEL_6_7,
            D3D_SHADER_MODEL_6_6,
            D3D_SHADER_MODEL_6_5,
            D3D_SHADER_MODEL_6_0,
            D3D_SHADER_MODEL_5_1,
        };
        for (D3D_SHADER_MODEL sm : sm_candidates)
        {
            D3D12_FEATURE_DATA_SHADER_MODEL data{};
            data.HighestShaderModel = sm;
            if (SUCCEEDED(device->CheckFeatureSupport(
                D3D12_FEATURE_SHADER_MODEL, &data, sizeof(data))))
            {
                caps.max_shader_model = data.HighestShaderModel;
                break;
            }
        }

        // --- Resource Binding Tier / Heap Tier ---
        D3D12_FEATURE_DATA_D3D12_OPTIONS opts{};
        if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS, &opts, sizeof(opts))))
        {
            caps.binding_tier = static_cast<UINT>(opts.ResourceBindingTier);
            caps.heap_tier    = static_cast<UINT>(opts.ResourceHeapTier);
        }

        // --- Wave Ops ---
        D3D12_FEATURE_DATA_D3D12_OPTIONS1 opts1{};
        if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS1, &opts1, sizeof(opts1))))
        {
            caps.wave_ops = opts1.WaveOps != FALSE;
            caps.wave_min = opts1.WaveLaneCountMin;
            caps.wave_max = opts1.WaveLaneCountMax;
        }

        // --- Mesh Shader Tier ---
        D3D12_FEATURE_DATA_D3D12_OPTIONS7 opts7{};
        if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS7, &opts7, sizeof(opts7))))
        {
            caps.mesh_shader_tier = static_cast<UINT>(opts7.MeshShaderTier);
        }

        // --- AtomicInt64 on Typed Resource ---
        D3D12_FEATURE_DATA_D3D12_OPTIONS9 opts9{};
        if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS9, &opts9, sizeof(opts9))))
        {
            caps.atomic_int64_typed =
                opts9.AtomicInt64OnTypedResourceSupported != FALSE;
        }

        // --- Variable Rate Shading Tier ---
        D3D12_FEATURE_DATA_D3D12_OPTIONS6 opts6{};
        if (SUCCEEDED(device->CheckFeatureSupport(
            D3D12_FEATURE_D3D12_OPTIONS6, &opts6, sizeof(opts6))))
        {
            caps.vrs_tier = static_cast<UINT>(opts6.VariableShadingRateTier);
        }

        return caps;
    }

    void log_caps(const DeviceCaps& caps)
    {
        std::printf("Device capabilities:\n");
        std::printf("    Max Feature Level:        %s\n",
            feature_level_str(caps.max_feature_level));
        std::printf("    Max Shader Model:         %s\n",
            shader_model_str(caps.max_shader_model));
        std::printf("    Resource Binding Tier:    %u%s\n",
            caps.binding_tier,
            caps.binding_tier >= 3 ? "      [BINDLESS-CAPABLE]" : "");
        std::printf("    Resource Heap Tier:       %u\n", caps.heap_tier);
        std::printf("    Wave Ops:                 %s%s\n",
            caps.wave_ops ? "yes" : "no",
            caps.wave_ops ? "" : "");
        if (caps.wave_ops)
        {
            std::printf("      Lane count:             %u..%u\n",
                caps.wave_min, caps.wave_max);
        }
        std::printf("    AtomicInt64 on typed RT:  %s\n",
            caps.atomic_int64_typed ? "yes" : "no");
        std::printf("    Mesh Shader Tier:         %u%s\n",
            caps.mesh_shader_tier,
            caps.mesh_shader_tier >= 1 ? "      [MESH-SHADER-CAPABLE]" : "");
        std::printf("    Variable Rate Shading:    Tier %u\n", caps.vrs_tier);
    }

    bool check_nanite_gate(const DeviceCaps& caps)
    {
        const bool sm_ok    = caps.max_shader_model >= D3D_SHADER_MODEL_6_6;
        const bool tier_ok  = caps.binding_tier     >= 3;
        const bool pass     = sm_ok && tier_ok;
        std::printf("Nanite gate (SM>=6.6 AND BindingTier>=3): %s\n",
            pass ? "PASS" : "FAIL");
        if (!pass)
        {
            std::fprintf(stderr,
                "  Got:  SM %s, BindingTier %u\n"
                "  Need: SM 6.6+, BindingTier 3+\n"
                "  Adapter likely not Nanite-capable. Exiting.\n",
                shader_model_str(caps.max_shader_model), caps.binding_tier);
        }
        return pass;
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

    // === DXGI Factory ===
    ComPtr<IDXGIFactory6> factory;
    HRESULT hr = CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
    if (FAILED(hr))
    {
        std::fprintf(stderr, "CreateDXGIFactory2 failed (hr=0x%08lX)\n",
            static_cast<unsigned long>(hr));
        glfwTerminate();
        return 1;
    }

    // === 选 adapter ===
    AdapterChoice chosen;
    if (!pick_adapter(factory.Get(), chosen))
    {
        glfwTerminate();
        return 1;
    }

    // === 创建 D3D12 Device ===
    ComPtr<ID3D12Device> device = create_device(chosen.adapter.Get());
    if (!device)
    {
        glfwTerminate();
        return 1;
    }

    // === 探测能力 + Nanite 门槛检查 ===
    DeviceCaps caps = query_caps(device.Get());
    log_caps(caps);
    if (!check_nanite_gate(caps))
    {
        glfwTerminate();
        return 1;
    }

    // === 窗口（沿用） ===
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720,
                                          "WNanite — step 05",
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

## 6. 预期 stdout

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
    ...
[2] Microsoft Basic Render Driver
    ...
    Flags: 0x00000002 [SOFTWARE]
...
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

具体能力按机器变化；RTX 5060 应该都 PASS。

## 7. 验证

- **编译**：Debug + Release 双 preset 通过，`main.cpp` `/W4 /permissive-` 零警告
- **运行**：stdout 含完整流水线输出 + `Nanite gate: PASS` + 窗口可开/可关 + `EXIT:0`
- **回归**：步 04 DXGI 区块仍输出，行为不变
- **故意失败测试**（spec 验证设计的鲁棒性，不在 plan 里强制做）：把 `pick_adapter` 的 `!is_software` 改成 `is_software` 强制选 WARP → 预期 `Nanite gate: FAIL` + exit 1
- 不写单测；无 golden image；无 ai-learn

## 8. UE5 对照（关键引用）

- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Device.cpp:173-200` — `FindHighestShaderModel`：从高到低试 SM 候选，第一个 SUCCESS 即 max
- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\D3D12Adapter.cpp:1076` — 注释明确：`ResourceDescriptorHeap/SamplerDescriptorHeap must be supported on devices that support both D3D12_RESOURCE_BINDING_TIER_3 and D3D_SHADER_MODEL_6_6`
- `D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\D3D12Adapter.cpp:1350` — Nanite 实际门槛：`MaxSupportedFeatureLevel >= D3D_FEATURE_LEVEL_12_0 && MaxSupportedShaderModel >= D3D_SHADER_MODEL_6_6 && ResourceBindingTier >= D3D12_RESOURCE_BINDING_TIER_3`

`ue5-refs.md` 会完整记录这些。

## 9. 风险

| 风险 | 影响 | 缓解 |
|---|---|---|
| 用户机器 SM < 6.6（老 GTX 10 系列等） | Nanite gate FAIL | 清晰报错 + exit 1，比 silent crash 好 |
| `CheckFeatureSupport` 对新结构体不支持 | E_INVALIDARG，对应字段保留默认值 | 全用 `SUCCEEDED(...)` 包裹，失败保留默认。所选用的 OPTIONS / OPTIONS1 / OPTIONS6 / OPTIONS7 / OPTIONS9 都在 Win10 1903+ 稳定 |
| 硬件 SM > 6.7 但 SDK 22621 看不见 | `query_caps` 报告的 max SM 上限被 SDK 卡在 6.7（即使 RTX 5060 实际支持 6.8）| 不影响 Nanite gate（≥6.6 即过）。如要看到真上限，需升级到 Windows SDK 10.0.26100+ 或集成 D3D12 Agility SDK——留作后置改进 |
| Adapter 真有 0 张（极端 VM 无 GPU 无 WARP） | `pick_adapter` 失败 + exit 1 | 应该不会发生（WARP 至少在；但因为我们跳 software，确实 0 张物理 GPU 时会 FAIL）。学习项目门槛接受 |

## 10. 范围外（明确）

- 不开 Debug Layer（步 06）
- 不创 Command Queue / Allocator / List（步 07）
- 不创 SwapChain（步 08）
- 不做 adapter 选择 UI / 命令行覆盖（YAGNI；步 ≥ 30 imgui 接入后再考虑）
- 不去重同 LUID 多 adapter（步 04 已识别该问题）
- 不持久化 device / chosen adapter 到全局（学习项目 main 持有，作用域随 main 退出）
- 不查询：D3D12_FEATURE_DATA_ROOT_SIGNATURE_HIGHEST_VERSION / D3D12_FEATURE_DATA_D3D12_OPTIONS19+ / Sampler Feedback Tier / Raytracing Tier 等（学习项目本步够用）

## 11. 下一步流程

1. 用户审阅
2. 调 `superpowers:writing-plans` 出实施计划
3. inline 执行 + 验证 + Docs + step-05 commit + push origin
