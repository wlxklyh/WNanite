# 从零写 DX12 Nanite 沙盒 (2)：GLFW 窗口 + DX12 第一次握手

> [上一篇](../articles/2026-05-20-phase-a-bootstrap-cmake-glfw.md)：CMake 骨架 + 引入 GLFW（链接通畅，未开窗口）
>
> 这一篇覆盖 Phase A 步骤 03+04——从 OS 真窗口出场到 DX12 生态第一次握手。两步加一起约 200 行代码，但每一行都有"为什么这么写"。

---

## 这一篇要做什么

承接上篇：项目骨架立起来了，GLFW 已链上并 `glfwInit` / `glfwGetVersionString` / `glfwTerminate` 验证通过。但还**没有真正的窗口**——一个能点 ✕ 能 ESC 退出的窗口。

这一篇推进两步：

| 步 | 做什么 | 验收信号 |
|---|---|---|
| 03 | GLFW 窗口生命周期 + 事件循环 | 1280×720 窗口可开/可关 |
| 04 | DXGI Factory + Adapter 枚举 | stdout 列出所有 GPU |

两步加起来形成一条剧情线：**从 OS 视角的窗口出场 → DX12 生态的第一次自我介绍**。仍**没**任何渲染 API 调用——swap chain、device、清屏都留到后续 Phase。但 DX12 生态的"探测层"已经接通，下一篇就要碰 `ID3D12Device` 了。

---

## 步骤 03：GLFW 窗口生命周期 + 事件循环

### 一行代码定生死

整步最值钱的一行：

```cpp
glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
```

GLFW 默认行为是**给每个窗口创建 OpenGL 上下文**。但我们将来要用 DX12，意味着：

- OpenGL 上下文会占用窗口的 device context
- DX12 通过 HWND 创建 swap chain 时**可能与 GL 上下文冲突**
- 即使不冲突，多一份未用上下文是浪费 + 调试噪声

`GLFW_NO_API` 明确告诉 GLFW："我自己处理图形 API，你只负责窗口"。**漏写这一行后果是步 04 DX12 设备创建时会出现 silent 失败或行为异常**——这种 bug 极难定位，所以**第一次开窗口就显式声明**。

### 完整 main.cpp（仅本步关键部分）

```cpp
#include <GLFW/glfw3.h>
#include <cstdio>

namespace
{
    // error callback：写 stderr。
    // 后续步 12 spdlog 接入后改为 spdlog::error。
    void on_glfw_error(int code, const char* description)
    {
        std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
    }

    // ESC 键 → 请求关闭窗口。
    void on_glfw_key(GLFWwindow* window, int key, int /*scancode*/,
                     int action, int /*mods*/)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
            glfwSetWindowShouldClose(window, GLFW_TRUE);
    }
}

int main()
{
    // 错误回调必须在 glfwInit 之前注册——
    // init 自身的错误也能拿到。
    glfwSetErrorCallback(on_glfw_error);
    if (!glfwInit()) return 1;

    // 关键：禁止 GLFW 默认创建 OpenGL 上下文。
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(1280, 720,
                                          "WNanite — step 03",
                                          nullptr, nullptr);
    glfwSetKeyCallback(window, on_glfw_key);

    while (!glfwWindowShouldClose(window))
        glfwPollEvents();

    glfwDestroyWindow(window);
    glfwTerminate();
}
```

### 几个关键决定的为什么

**为什么 callback 注册在 `glfwInit` 之前**：

`glfwInit` 自己也可能产生错误（找不到 X server、Win32 API 调用失败等）。如果在 init 之后才设回调，init 失败的错误就 silent 丢了。**规则：error callback 永远第一个注册**。

**为什么 callback 放在匿名 namespace**：

C 风格回调（函数指针签名）必须用 `extern "C"` 友好的自由函数，**lambda 不能直接做 GLFW 回调**（即使无捕获 lambda 编译能过，也容易引入 ABI 不一致问题）。匿名 namespace 防止与其他 translation unit 同名函数 ODR 冲突，表达"内部链接"语义。

**为什么 ESC 退出走 key callback 而不是主循环里轮询**：

GLFW 主循环里用 `glfwGetKey(window, GLFW_KEY_ESCAPE)` 轮询也行，但 callback 是**事件驱动**——按下的瞬间响应，没有"按了一帧没被 poll 到"的边界条件。后续相机输入会同时用 callback（离散事件）+ 轮询（连续状态），本步先建 callback 路径。

**为什么 destroy 在 terminate 之前**：

GLFW 的资源拥有关系：`Window` 属于 GLFW 全局状态。`glfwTerminate` 会自动销毁所有窗口，但显式 destroy 让生命周期清晰，也方便后续多窗口场景。

**为什么主循环不 sleep**：

`glfwPollEvents` 立即返回，CPU 单核会接近 100%。**本步不解决**——swap chain `Present` 引入后会自带垂直同步等待（Phase B+）。学习项目里"明知问题但暂不解决"也是态度，比"提前优化但不知道为什么"健康。

### ESC 触发的微妙时序

```cpp
glfwSetWindowShouldClose(window, GLFW_TRUE);
```

**这只是设标志位**——窗口不会立刻消失。下一次 `while` 迭代检查 `glfwWindowShouldClose` 时才退出循环。听起来废话，但它有教学意义：**渲染主循环的"退出"永远是异步的**——你只能"请求"退出，实际清理在主循环检测后才发生。后续 DX12 fence 处理 device removed 时这个思维模式会反复遇到。

---

## 步骤 04：DXGI Factory + Adapter 枚举

### 这一步看起来很无聊

代码 120 行，全部输出是几行文字：

```
DXGI adapters (by HIGH_PERFORMANCE):
[0] NVIDIA GeForce RTX 5060
    Vendor: NVIDIA (0x10DE)  Device: 0x2D05
    VRAM: 7895 MB  Shared: 31516 MB
    LUID: 0x00000000:0x00012AC8
    Flags: 0x00000000
...
```

但里面藏的学习点密度极高——8 个深度话题。挑最值得讲的几个。

### 为什么先 DXGI 不 D3D12

DXGI 与 D3D12 是**两层独立的 API**：

| 层 | 管什么 | 谁拥有 |
|---|---|---|
| **DXGI** | 系统视角：adapters / outputs / swap chains / display modes | OS+驱动 |
| **D3D12** | 设备视角：device / cmdlist / resources / PSO | 你创建的 device |

DXGI 不需要 device 就能存在。架构上 DXGI 本来就独立——先用它探明所有显卡，下一步再选一张创 device。后面 swap chain（DXGI 创但服务 D3D12 device）的"跨层"API 看起来奇怪，理解了这个分层就合理。

### ComPtr / IID_PPV_ARGS — 整个 DX12 代码的基础语法

```cpp
ComPtr<IDXGIFactory6> factory;
CreateDXGIFactory2(0, IID_PPV_ARGS(&factory));
```

两个关键展开：

**`IID_PPV_ARGS(&factory)`** 宏展开成：

```cpp
__uuidof(IDXGIFactory6), reinterpret_cast<void**>(&factory)
```

一举两得——拿对应 IID + 拿 `void**` 输出槽位。**全 DX12 代码都用这俩参数对**，弄懂它你就能读懂 90% 的对象创建调用。

**`ComPtr<T>::operator&()`** 不是普通取地址：

- 先 `Release()` 旧引用
- 再返回 `T**`

这就是为什么 RAII 不漏 release 的关键。整个 DX12 ecosystem 都用这套语法——`ID3D12Device::CreateXxx(... IID_PPV_ARGS(&ptr))`。

### `EnumAdapterByGpuPreference` 与多 GPU 的历史

DXGI 1.6 之前用 `EnumAdapters1`，顺序由驱动决定——**笔记本独显+核显场景经常默认枚到核显**（Intel UHD）。1.6 加 `EnumAdapterByGpuPreference` 才能稳定按性能排。

我们用：

```cpp
factory->EnumAdapterByGpuPreference(
    i,
    DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,
    IID_PPV_ARGS(&adapter));
```

`DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE` 把独显排到第一。

UE D3D12RHI 的做法更工业级（`Engine/Source/Runtime/D3D12RHI/Private/Windows/WindowsD3D12Device.cpp:813-863`）：**同时拿 Factory4 + Factory6**——新 Factory6 走 `EnumAdapterByGpuPreference`，老 Factory4 走 `EnumAdapters` 兼容降级。学习项目把降级路径砍掉就好，门槛卡 Win10 1803+ 没问题。

### HRESULT 与 `DXGI_ERROR_NOT_FOUND` 哨兵

```cpp
for (UINT i = 0;; ++i) {
    hr = factory->EnumAdapterByGpuPreference(i, ..., &adapter);
    if (hr == DXGI_ERROR_NOT_FOUND) break;  // 不是错误！是"枚完了"
    if (FAILED(hr)) return false;            // 真错误
    // ... 处理 adapter ...
}
```

`DXGI_ERROR_NOT_FOUND` 是 COM 风格 API 的**友好失败哨兵**——`SUCCEEDED`/`FAILED` 宏不够区分"枚到底"和"真错"，所以用一个具体错误码作为循环终止信号。

HRESULT 内部是 32-bit packed（severity + facility + code）。打印用 `0x%08lX` 看具体码，查官方错误对照表能定位问题。

### WARP 是什么 / 为什么列表里有

输出里有：

```
[2] Microsoft Basic Render Driver
    Vendor: Microsoft/WARP (0x1414)  Device: 0x008C
    VRAM: 0 MB  Shared: 31516 MB
    Flags: 0x00000002 [SOFTWARE]
```

WARP = Windows Advanced Rasterization Platform = **纯 CPU 实现的 D3D12 驱动**。

- 跑得动 D3D11/D3D12 大多数 API（含基础 Compute）
- **但没硬件能力**：无 `BindlessResourceDescriptor`（SM 6.6 必需）、无 `AtomicInt64`、极慢
- 用途：CI、服务器、无 GPU VM、调试驱动行为
- **Nanite 完全跑不动 WARP**——这是后续"必须真 GPU"的判别

### 实战发现：同一物理 GPU 被枚成多个 adapter

这是 README 里**绝对不能漏**的一段——只有真跑过才会撞上：

```
[0] NVIDIA GeForce RTX 5060  LUID: 0x00012AC8  ← 主独显
[1] AMD Radeon(TM) Graphics  LUID: 0x00013E40  ← 核显
[2] Microsoft Basic Render Driver           ← WARP
[3] NVIDIA GeForce RTX 5060  LUID: 0x000809EE  ← 同卡，不同 LUID
[4] NVIDIA GeForce RTX 5060  LUID: 0x00082210  ← 同卡，又一个 LUID
```

**NVIDIA 驱动给同一物理 RTX 5060 暴露 3 个 DXGI adapter**（不同 LUID，相同 description/vendor/device/VRAM）。可能的原因：

- **D3D12 multi-GPU linked-node**：一张物理 GPU 内多个 node，每个 node 在 DXGI 层是独立 adapter
- **DCH 驱动**：Studio / Game-Ready 驱动为 OptiX / CUDA / DX 各自暴露独立 adapter
- **Hybrid GPU 路径**：Windows OS 混合渲染（Optimus 续作）相关

**实战教训**：

- 不能只看 adapter 数量推断"机器上有几张 GPU"
- 选 adapter 时只看 description 不够（同名 3 个），**必须靠 LUID + Flags + GpuPreference 排序联合判断**
- UE 用 LUID 去重（`D3D12Adapter.cpp` 里有相关逻辑）

这就是文档不会告诉你的——只有真跑过 `EnumAdapterByGpuPreference` 看实机输出才会撞上的。

### `printf("%S", desc.Description)` 的跨平台坑

`DXGI_ADAPTER_DESC1::Description` 是 `WCHAR[128]`（UTF-16 LE）。`printf` 里：

- **MSVC**：`%S`（大写）= WCHAR\*，`%s`（小写）= char\*
- **glibc/clang POSIX**：**反过来**——`%s` = wchar_t\*，`%S` 是别的东西

跨平台代码不能直接 `%S`。我们的项目 Windows-only + `/utf-8` + 控制台 `chcp 65001`，三个条件凑齐才不乱码。一个跨平台代码迁移时的隐藏陷阱。

---

## 与 UE 对照：工业代码 vs 学习代码

直接对照能看清"为什么 UE 那么复杂"。

UE 的 adapter 枚举（精简）：

```cpp
// D:\LYH\UE\Engine\Source\Runtime\D3D12RHI\Private\Windows\WindowsD3D12Device.cpp:838
TRefCountPtr<IDXGIAdapter> TempAdapter;
for (uint32 i = 0;
     FD3D12AdapterDesc::EnumAdapters(i, GpuPreference,
         DXGIFactory4, DXGIFactory6, TempAdapter.GetInitReference())
         != DXGI_ERROR_NOT_FOUND;
     ++i)
{ ... }
```

WNanite：

```cpp
ComPtr<IDXGIAdapter1> adapter;
for (UINT i = 0;
     factory->EnumAdapterByGpuPreference(i, ..., IID_PPV_ARGS(&adapter))
         != DXGI_ERROR_NOT_FOUND;
     ++i)
{ ... }
```

**骨架完全一致**。差别只有两处：

1. UE 在外面再包了 `FD3D12AdapterDesc::EnumAdapters` 做 Factory4/6 双路径兼容降级
2. UE 用 `TRefCountPtr::GetInitReference()`，我们用 `ComPtr::operator&()`——语义完全等价

UE 的工业级复杂性主要来自**多平台 + 多 RHI + 老硬件兼容**。学习项目把这些剔掉，剩下的骨架 = 你能完整看清楚的算法本身。

**这种"对照看"是这个学习项目最值钱的产出**——以后做 Nanite 算法实现时，UE 的 `Nanite.cpp` 复杂得像迷宫，但只要先在学习项目里写过一个简化版，再去看 UE 就完全不一样了。

---

## 一些反思

这两步加起来 ~200 行代码，但写了：

- 步 03 设计文档 + 实施计划 + 学习文档（READMEs ~400 行）+ UE 对照
- 步 04 设计文档 + 实施计划 + 学习文档（READMEs ~600 行，含 8 深度话题 + 7 个踩过的坑）+ UE 对照
- 文档/代码比例：**大约 5:1**

这是我喜欢这种学习节奏的关键——**文档 ≫ 代码 是 feature 不是 bug**。代码是"知道答案"，文档是"理解答案"。两年后回来如果只有代码、没有文档，等于零。

CLAUDE.md 这次还顺手加了一条原则：**"遇事不决查 UE5"**——本地有 UE5 源码就用，所有 file:line 引用记入 `Docs/NN-<topic>/ue5-refs.md`，但绝不复制 UE5 代码（License）。步 04 第一次落地这条规则——8 个学习点里有 2 个就是基于 UE 源码对照写的。

---

## 下一篇预告

**步 05：`D3D12 Device` 创建 + Feature Level / SM 检测**

终于要碰 `D3D12CreateDevice` 了——选一张 adapter（默认 `[0]`），创建 `ID3D12Device`，调 `CheckFeatureSupport` 查 SM 6.6 / Bindless / AtomicInt64 是否支持。若 SM < 6.6 立即报错退出（**Nanite 的硬性门槛**）。

至此 DX12 才真正"进入主舞台"。

---

> 本项目地址：[github.com/wlxklyh/WNanite](https://github.com/wlxklyh/WNanite)
> 本文对应 commits：`920a54f` (step-03) / `9526834` (CLAUDE rule) / `a8e42f8` (step-04)
> 系列计划：46 步搭 harness（子项目 0）→ 6 个子项目复刻 UE5 Nanite 主路径
> 每步 < 1 小时，跑通就提交，慢工出细活
