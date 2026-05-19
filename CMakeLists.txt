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

# === 第三方依赖（FetchContent） ===
include(FetchContent)

# --- GLFW 3.4 ---
# 关掉 GLFW 自带的非核心子目标，缩短首次 configure 时间且避免污染目标列表。
set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG        3.4
    GIT_SHALLOW    TRUE
    SYSTEM                          # CMake 3.25+：把 GLFW include 路径标为 SYSTEM，屏蔽其内部 /W4 警告
)
FetchContent_MakeAvailable(glfw)

# === 可执行目标 ===
add_executable(wnanite
    src/main.cpp
)

# === 链接第三方 ===
target_link_libraries(wnanite PRIVATE glfw dxgi)
