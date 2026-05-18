# Step 01 — CMake Hello-World Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` (recommended) or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让仓库具备最小可编译可运行的 C++ 骨架——CMake 配置成功、构建成功、运行后 stdout 输出 `hello, WNanite!`。同时完成仓库 `git init` 与第一次提交。

**Architecture:** 单一可执行目标 `wnanite`，源码 `src/main.cpp`，构建走 CMake Presets（Debug / Release 各一个 preset，分别输出到 `build-debug/` 与 `build-release/`）。本步**不引入任何第三方依赖、不引入 DX12 / GLFW**，纯粹打骨架。

**Tech Stack:** CMake ≥ 3.25，MSVC（Visual Studio 2022），C++20，Windows 11。

**对应 Spec：** `Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md` §5 Phase A 步 01。

---

## File Structure

```
WNanite/
  .gitignore                          # 新建
  CMakeLists.txt                      # 新建
  CMakePresets.json                   # 新建
  src/
    main.cpp                          # 新建（仅 print hello；后续步会拆分）
  CLAUDE.md                           # 已存在 — Task 7 勾选进度
  Docs/
    01-cmake-hello/                   # 新建目录
      README.md                       # 新建（7 节学习笔记）
      ue5-refs.md                     # 新建（本步无 UE5 对应，注明）
      screenshots/                    # 新建（本步无可见输出，保留空目录占位）
        .gitkeep
    superpowers/
      specs/
        2026-05-19-wnanite-harness-design.md   # 已存在
      plans/
        2026-05-19-step-01-cmake-hello.md      # 本文件
  build-debug/                        # cmake 生成，被 .gitignore 排除
  build-release/                      # 同上
```

**单文件职责说明：**
- `CMakeLists.txt`：项目元数据、C++20 标准、MSVC 编译选项、单一可执行目标声明
- `CMakePresets.json`：Debug / Release 两个 configurePreset + 对应 buildPreset
- `src/main.cpp`：仅 `int main` + 输出一行
- `.gitignore`：排除构建产物、IDE 元数据、日志

---

## Task 1: 仓库初始化（git init + 初次提交）

**Files:**
- Create: `E:\LYH\WNanite\.gitignore`
- 已存在并将提交：`CLAUDE.md`、`Docs/superpowers/specs/2026-05-19-wnanite-harness-design.md`、`Docs/superpowers/plans/2026-05-19-step-01-cmake-hello.md`

- [ ] **Step 1.1: 检查仓库目前不是 git 仓库**

Run（PowerShell）:
```powershell
Test-Path E:\LYH\WNanite\.git
```
Expected: `False`

- [ ] **Step 1.2: `git init`**

Run:
```powershell
git -C E:\LYH\WNanite init -b main
```
Expected: `Initialized empty Git repository in E:/LYH/WNanite/.git/`

- [ ] **Step 1.3: 创建 `.gitignore`**

Create `E:\LYH\WNanite\.gitignore` with exactly:
```gitignore
# Build outputs
build/
build-debug/
build-release/
out/

# Runtime artifacts
logs/
*.log
*.wpix
*.rdc

# Editor / IDE
.vs/
.vscode/
.idea/
*.user
*.suo
*.sdf
*.opensdf

# CMake
CMakeUserPresets.json
CMakeCache.txt
CMakeFiles/

# OS
Thumbs.db
.DS_Store
```

- [ ] **Step 1.4: 校验 `.gitignore` 生效（构建目录将被忽略）**

Run:
```powershell
git -C E:\LYH\WNanite check-ignore -v build-debug/ logs/x.log
```
Expected: 两行命中规则（`.gitignore:3:build-debug/` 类似输出）。

- [ ] **Step 1.5: 暂存当前已有文件**

Run:
```powershell
git -C E:\LYH\WNanite add .gitignore CLAUDE.md Docs/
```

- [ ] **Step 1.6: 校验暂存内容（不应包含 build-* / logs/）**

Run:
```powershell
git -C E:\LYH\WNanite status --short
```
Expected: 仅看到 `A  .gitignore`、`A  CLAUDE.md` 以及 `A  Docs/...` 三类条目；没有 `build-*` 或 `logs/`。

- [ ] **Step 1.7: 初次提交**

Run:
```powershell
git -C E:\LYH\WNanite commit -m "init: repo with design spec + CLAUDE.md"
```
Expected: `[main (root-commit) <sha>] init: repo with design spec + CLAUDE.md` + 3+ files changed。

---

## Task 2: Hello-World 源文件

**Files:**
- Create: `E:\LYH\WNanite\src\main.cpp`

- [ ] **Step 2.1: 创建 src 目录**

Run:
```powershell
New-Item -ItemType Directory -Force -Path E:\LYH\WNanite\src | Out-Null
```

- [ ] **Step 2.2: 写 `src/main.cpp`**

Create `E:\LYH\WNanite\src\main.cpp` with exactly:
```cpp
// WNanite — 主入口
// 当前阶段：步 01 — 仅打印一行以验证 CMake/MSVC 工具链通畅。
// 后续步会逐步引入 GLFW 窗口（步 02）、DX12 设备（步 04+）。

#include <cstdio>

int main()
{
    std::printf("hello, WNanite!\n");
    return 0;
}
```

- [ ] **Step 2.3: 校验文件内容（行数 / 包含关键字）**

Run:
```powershell
(Get-Content E:\LYH\WNanite\src\main.cpp -Raw).Contains("hello, WNanite!")
```
Expected: `True`

---

## Task 3: CMakeLists.txt

**Files:**
- Create: `E:\LYH\WNanite\CMakeLists.txt`

- [ ] **Step 3.1: 写 `CMakeLists.txt`**

Create `E:\LYH\WNanite\CMakeLists.txt` with exactly:
```cmake
# WNanite — root build script
# 步 01：最小可编译骨架，仅一个可执行目标。

cmake_minimum_required(VERSION 3.25)

project(WNanite
    LANGUAGES CXX
    DESCRIPTION "DX12 Nanite-style learning sandbox"
)

# === 全局 C++ 标准 ===
# Nanite 风格代码大量用到 concepts / ranges / consteval，C++20 是底线。
set(CMAKE_CXX_STANDARD          20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS        OFF)  # 关掉编译器扩展，写更可移植的标准 C++

# === MSVC 专属编译选项 ===
# /W4              较高警告级别（学习项目尽量早暴露问题）
# /utf-8           源文件按 UTF-8 解析（PowerShell 控制台 + 中文注释需要）
# /Zc:__cplusplus  让 __cplusplus 宏反映真实标准（MSVC 默认是 199711L 的坑）
# /Zc:preprocessor 启用符合标准的预处理器（旧版 MSVC 的不一致预处理器是另一个坑）
# /permissive-     更严格的标准合规
if(MSVC)
    add_compile_options(
        /W4
        /utf-8
        /Zc:__cplusplus
        /Zc:preprocessor
        /permissive-
    )
endif()

# === 可执行目标 ===
add_executable(wnanite
    src/main.cpp
)
```

- [ ] **Step 3.2: 校验文件存在**

Run:
```powershell
Test-Path E:\LYH\WNanite\CMakeLists.txt
```
Expected: `True`

---

## Task 4: CMakePresets.json

**Files:**
- Create: `E:\LYH\WNanite\CMakePresets.json`

- [ ] **Step 4.1: 写 `CMakePresets.json`**

Create `E:\LYH\WNanite\CMakePresets.json` with exactly:
```json
{
  "version": 6,
  "cmakeMinimumRequired": { "major": 3, "minor": 25, "patch": 0 },
  "configurePresets": [
    {
      "name": "debug",
      "displayName": "Debug (MSVC)",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build-debug",
      "cacheVariables": {
        "CMAKE_CONFIGURATION_TYPES": "Debug"
      }
    },
    {
      "name": "release",
      "displayName": "Release (MSVC)",
      "generator": "Visual Studio 17 2022",
      "binaryDir": "${sourceDir}/build-release",
      "cacheVariables": {
        "CMAKE_CONFIGURATION_TYPES": "Release"
      }
    }
  ],
  "buildPresets": [
    { "name": "debug",   "configurePreset": "debug",   "configuration": "Debug"   },
    { "name": "release", "configurePreset": "release", "configuration": "Release" }
  ]
}
```

- [ ] **Step 4.2: 校验 JSON 格式正确**

Run:
```powershell
Get-Content E:\LYH\WNanite\CMakePresets.json -Raw | ConvertFrom-Json | Select-Object -ExpandProperty version
```
Expected: `6`

---

## Task 5: 构建 + 运行验证（Debug）

- [ ] **Step 5.1: 配置 Debug preset**

Run（在 `E:\LYH\WNanite` 目录下）:
```powershell
cmake --preset debug
```
Expected: 末尾出现 `-- Configuring done` / `-- Generating done` / `-- Build files have been written to: E:/LYH/WNanite/build-debug`，退出码 0。

若失败常见原因：
- VS 2022 未安装 → 报 generator 找不到
- 编译器找不到 → 在 "Developer PowerShell for VS 2022" 中再次运行

- [ ] **Step 5.2: 构建 Debug**

Run:
```powershell
cmake --build build-debug --config Debug -j
```
Expected: 末尾出现 `wnanite.vcxproj -> E:\LYH\WNanite\build-debug\Debug\wnanite.exe`，退出码 0，无 warning（/W4 下 main.cpp 应零警告）。

- [ ] **Step 5.3: 运行 Debug 二进制**

Run:
```powershell
& E:\LYH\WNanite\build-debug\Debug\wnanite.exe
```
Expected: stdout 恰好一行 `hello, WNanite!`，退出码 0（`$LASTEXITCODE` 应为 0）。

- [ ] **Step 5.4: 校验退出码**

Run（紧跟上一步）:
```powershell
$LASTEXITCODE
```
Expected: `0`

---

## Task 6: 构建 + 运行验证（Release）

- [ ] **Step 6.1: 配置 Release preset**

Run:
```powershell
cmake --preset release
```
Expected: 同 5.1，输出到 `build-release/`。

- [ ] **Step 6.2: 构建 Release**

Run:
```powershell
cmake --build build-release --config Release -j
```
Expected: `wnanite.exe` 生成到 `E:\LYH\WNanite\build-release\Release\wnanite.exe`，退出码 0。

- [ ] **Step 6.3: 运行 Release 二进制**

Run:
```powershell
& E:\LYH\WNanite\build-release\Release\wnanite.exe; $LASTEXITCODE
```
Expected: 输出 `hello, WNanite!` 一行 + 退出码 `0`。

---

## Task 7: 学习文档 — `Docs/01-cmake-hello/`

**Files:**
- Create: `Docs/01-cmake-hello/README.md`
- Create: `Docs/01-cmake-hello/ue5-refs.md`
- Create: `Docs/01-cmake-hello/screenshots/.gitkeep`

- [ ] **Step 7.1: 创建目录**

Run:
```powershell
New-Item -ItemType Directory -Force -Path E:\LYH\WNanite\Docs\01-cmake-hello\screenshots | Out-Null
```

- [ ] **Step 7.2: 写 `.gitkeep` 占位**

Create `E:\LYH\WNanite\Docs\01-cmake-hello\screenshots\.gitkeep` with content（空文件，作占位）:
```
```

- [ ] **Step 7.3: 写 `README.md`（七节模板）**

Create `E:\LYH\WNanite\Docs\01-cmake-hello\README.md` with exactly:
````markdown
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

## 7. 下一步预告

**步 02：引入 GLFW**——通过 CMake FetchContent 拉 GLFW 源码、用 `target_link_libraries` 链接到 wnanite，验证第三方依赖管理通畅。仍不开窗口，仅链接成功 + `glfwInit()` 调用通过。
````

- [ ] **Step 7.4: 写 `ue5-refs.md`**

Create `E:\LYH\WNanite\Docs\01-cmake-hello\ue5-refs.md` with exactly:
```markdown
# UE5 源码对照 — 步 01

**本步 harness 实现与 UE5 不直接对应。**

UE5 使用自家 UnrealBuildTool（UBT）+ `.Build.cs` 文件定义模块与依赖，不使用 CMake。UE5 的构建入口约在：

- `Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.cs`
- 每个模块的 `*.Build.cs`（例如 `Engine/Source/Runtime/Renderer/Renderer.Build.cs`）

这套体系与 CMake 设计哲学不同（UBT 更面向"模块化引擎"，而 CMake 是通用构建系统）。本仓库是独立 DX12 沙盒，选用 CMake 是社区通用与 AI 工具链友好的考量，与"理解 UE5 Nanite 算法"这一学习目标无冲突——构建系统不是 Nanite 学习的核心。

后续真正进入 Nanite 算法实现（子项目 2+）时再大量引用 UE5 源码（`Engine/Source/Runtime/Renderer/Private/Nanite/*`）。
```

- [ ] **Step 7.5: 校验三个文档文件都已创建**

Run:
```powershell
@(
  "E:\LYH\WNanite\Docs\01-cmake-hello\README.md",
  "E:\LYH\WNanite\Docs\01-cmake-hello\ue5-refs.md",
  "E:\LYH\WNanite\Docs\01-cmake-hello\screenshots\.gitkeep"
) | ForEach-Object { Test-Path $_ }
```
Expected: 三个 `True`。

---

## Task 8: 更新 CLAUDE.md §10 进度

**Files:**
- Modify: `E:\LYH\WNanite\CLAUDE.md`（§10 Phase A 第 1 项）

- [ ] **Step 8.1: 勾选步 01**

Edit `E:\LYH\WNanite\CLAUDE.md`:
- old_string: `- [ ] 01 CMake 最小 hello-world`
- new_string: `- [x] 01 CMake 最小 hello-world — [Docs/01-cmake-hello](Docs/01-cmake-hello/README.md)`

- [ ] **Step 8.2: 校验勾选成功**

Run:
```powershell
Select-String -Path E:\LYH\WNanite\CLAUDE.md -Pattern "\[x\] 01 CMake"
```
Expected: 命中一行。

---

## Task 9: 最终提交（step-01）

- [ ] **Step 9.1: 暂存所有新增/修改**

Run:
```powershell
git -C E:\LYH\WNanite add .gitignore CMakeLists.txt CMakePresets.json src/ Docs/01-cmake-hello/ CLAUDE.md Docs/superpowers/plans/2026-05-19-step-01-cmake-hello.md
```

- [ ] **Step 9.2: 校验暂存（不含 build-* / logs/）**

Run:
```powershell
git -C E:\LYH\WNanite status --short
```
Expected: 仅 `A`/`M` 状态条目；**没有** `build-debug`、`build-release`、`logs/` 出现。

- [ ] **Step 9.3: 总结性提交**

Run:
```powershell
git -C E:\LYH\WNanite commit -m "step-01: CMake hello-world"
```
Expected: `[main <sha>] step-01: CMake hello-world` + 多个 files changed。

- [ ] **Step 9.4: 校验提交历史**

Run:
```powershell
git -C E:\LYH\WNanite log --oneline
```
Expected: 两行——
```
<sha> step-01: CMake hello-world
<sha> init: repo with design spec + CLAUDE.md
```

---

## Done 校验（对照 CLAUDE.md §3 单步完成定义）

- [ ] 1. Debug + Release 编译通过 — Task 5 / Task 6
- [ ] 2. 运行通过 — Step 5.3 / 6.3 输出 `hello, WNanite!`
- [ ] 3. 验收信号 — stdout + 退出码 0
- [ ] 4. `Docs/01-cmake-hello/README.md` 七节完整 — Task 7
- [ ] 5. ai-learn 不适用（API 流程类，按 §5.3 跳过） — N/A
- [ ] 6. doctest 不适用（无纯逻辑代码） — N/A
- [ ] 7. golden PNG 不适用（无可见输出） — N/A
- [ ] 8. CLAUDE.md §10 已勾 — Task 8

全部 ✓ 后步 01 完成，可进入步 02 brainstorming / writing-plans。
