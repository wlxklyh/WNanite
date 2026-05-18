# 步 01 — CMake Hello-World

> Phase A · Bootstrap · 第 1 步 / 共 11 步
> 对应 Spec：`Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md` §5 Phase A 步 01

---

## 1. 本步学了什么

为 WNanite 建立**最小可编译可运行的 C++ 骨架**：

- 用 CMake（≥ 3.25）声明一个项目和一个可执行目标
- 用 CMakePresets.json 标准化 Debug / Release 两种配置
- MSVC + C++20 标准 + 严格的编译选项（/W4 /permissive- /Zc:__cplusplus）
- `src/main.cpp` 仅打印一行字符串，验证工具链完整通畅

走完本步：`cmake --preset debug && cmake --build build-debug` 产出 `wnanite.exe`，运行后 stdout 输出 `hello, WNanite!`。

## 2. 为什么这么做

**为什么 CMake**：
- 跨 IDE（VS / VSCode / CLion / Rider）通吃
- 后续步 02 引入 GLFW、步 23 引入 DXC、cgltf、D3D12MA 等都用 FetchContent 拉，包管理省心
- AI 工具链最熟悉 CMake，比 xmake / 手写 .sln 更利于 agent 协作

**为什么 Presets**：
- 替代手写 `-DCMAKE_BUILD_TYPE=Debug -G "Visual Studio 17 2022" -B build-debug` 一长串
- `cmake --preset debug` 一句话搞定，CI / agent 都更稳

**为什么 C++20**：
- `concepts` 让 RDG / Bindless 模板类的接口约束清晰
- `consteval` / `constinit` 让 shader 反射/绑定表的元编程更安全
- `ranges` / `std::span` 让 mesh / index buffer 处理更简洁
- Nanite 风格代码里这些是常态

**为什么这一套 MSVC 编译选项**：
- `/W4`：学习项目，越早暴露问题越好
- `/utf-8`：源文件 UTF-8 解析；本仓库注释大量中文，必须显式声明
- `/Zc:__cplusplus`：MSVC 默认 `__cplusplus == 199711L`（历史遗留），加这个才能反映真实 C++20
- `/Zc:preprocessor`：启用符合标准的预处理器（旧的 MSVC 预处理器对宏展开有些非标行为）
- `/permissive-`：拒绝非标准扩展，写更可移植的代码

**为什么独立的 `build-debug/` 与 `build-release/`**：
- 两套配置可共存，切换无需 reconfigure
- 与 `.gitignore` 配合，构建产物完全不污染源码树

**没选什么 / 为什么**：
- 没选 xmake：生态略小，AI 协作不如 CMake 顺
- 没用 vcpkg：所有依赖统一用 CMake FetchContent，单源真理
- 没建 src/Core/ src/RHI/ 等子目录：YAGNI，等真要分模块（步 ≥ 03）再拆

## 3. 代码导读

| 文件 | 关键行 | 说明 |
|------|--------|------|
| `CMakeLists.txt:4` | `cmake_minimum_required(VERSION 3.25)` | preset v6 + FetchContent 现代特性的最低门槛 |
| `CMakeLists.txt:6-9` | `project(...)` | 项目名 / 语言 / 描述 |
| `CMakeLists.txt:13-15` | `set(CMAKE_CXX_STANDARD 20)` 等 | 强制 C++20 + 关编译器扩展 |
| `CMakeLists.txt:24-31` | `add_compile_options(...)` 包裹于 `if(MSVC)` | MSVC 严格选项（见 §2） |
| `CMakeLists.txt:34-36` | `add_executable(wnanite ...)` | 唯一可执行目标 |
| `CMakePresets.json` | 全文件 | Debug / Release 两个 preset，输出到 `build-debug/` 与 `build-release/` |
| `src/main.cpp:9` | `std::printf(...)` | 仅打印一行；用 `printf` 而非 `iostream` 是为了避免引入静态构造开销（学习习惯） |

## 4. UE5 是怎么做的

本步是构建系统层面的工作，UE5 用自家 UnrealBuildTool（UBT）+ `.Build.cs` 文件，与 CMake 不存在直接对应。UE5 不依赖 CMake。

详见 `ue5-refs.md`。

## 5. 截图 / GIF

本步无图形输出。`screenshots/` 留空（仅 `.gitkeep` 占位）。预期 stdout：

```
hello, WNanite!
```

## 6. 踩过的坑

- **`/W4` + `/permissive-` 下，`int main()` 不带 `void` 参数列表无 warning**——但若以后写 `int main(int argc, char* argv[])`，留意 MSVC 会要求精确签名
- **VS 2022 generator 找不到**：必须在装好 VS2022 的环境运行，或在 "Developer PowerShell for VS 2022" 里跑（普通 PowerShell 也行，前提是 VS 装好后会注册到 CMake 探测路径）
- **`CMAKE_BUILD_TYPE` 在 multi-config generator（VS）下无效**：所以在 preset 里用 `CMAKE_CONFIGURATION_TYPES` 而不是 `CMAKE_BUILD_TYPE`
- **PowerShell 调可执行文件必须用 `&`**：`& E:\path\wnanite.exe`，否则解析成裸字符串
- **git 首次 add 时的 LF→CRLF warning**：Windows 默认 `core.autocrlf=true`，对学习项目无害；若想严格统一可在仓库根加 `.gitattributes` 显式指定 `* text=auto eol=lf`

## 7. 下一步预告

**步 02：引入 GLFW**——通过 CMake FetchContent 拉 GLFW 源码、用 `target_link_libraries` 链接到 wnanite，验证第三方依赖管理通畅。仍不开窗口，仅链接成功 + `glfwInit()` 调用通过。
