// WNanite — 主入口
// 当前阶段：步 04 — DXGI Factory + Adapter 枚举。
// 第一次接 DX12;仍不创 D3D12 device（步 05），不开 Debug Layer（步 06）。

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
