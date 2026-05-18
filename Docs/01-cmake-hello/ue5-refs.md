# UE5 源码对照 — 步 01

**本步 harness 实现与 UE5 不直接对应。**

UE5 使用自家 UnrealBuildTool（UBT）+ `.Build.cs` 文件定义模块与依赖，不使用 CMake。UE5 的构建入口约在：

- `Engine/Source/Programs/UnrealBuildTool/UnrealBuildTool.cs`
- 每个模块的 `*.Build.cs`（例如 `Engine/Source/Runtime/Renderer/Renderer.Build.cs`）

这套体系与 CMake 设计哲学不同（UBT 更面向"模块化引擎"，而 CMake 是通用构建系统）。本仓库是独立 DX12 沙盒，选用 CMake 是社区通用与 AI 工具链友好的考量，与"理解 UE5 Nanite 算法"这一学习目标无冲突——构建系统不是 Nanite 学习的核心。

后续真正进入 Nanite 算法实现（子项目 2+）时再大量引用 UE5 源码（`Engine/Source/Runtime/Renderer/Private/Nanite/*`）。
