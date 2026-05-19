// WNanite — 主入口
// 当前阶段：步 03 — GLFW 窗口生命周期 + 事件循环。
// 仍不接 DX12（步 04+）；窗口背景由 OS 决定，没有任何渲染。

#include <GLFW/glfw3.h>
#include <cstdio>

namespace
{
    // 把 GLFW 报告的错误都写到 stderr。
    // 后续步 12 spdlog 接入后，会把这里改成 spdlog::error。
    void on_glfw_error(int code, const char* description)
    {
        std::fprintf(stderr, "GLFW error %d: %s\n", code, description);
    }

    // ESC 键 → 请求关闭窗口。
    void on_glfw_key(GLFWwindow* window, int key, int /*scancode*/,
                     int action, int /*mods*/)
    {
        if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }
    }
}

int main()
{
    std::printf("hello, WNanite!\n");

    // 错误回调要在 glfwInit 之前注册——init 自身的错误也能拿到。
    glfwSetErrorCallback(on_glfw_error);

    if (!glfwInit())
    {
        std::fprintf(stderr, "glfwInit failed\n");
        return 1;
    }
    std::printf("GLFW %s\n", glfwGetVersionString());

    // 关键：禁止 GLFW 自动创 OpenGL/OpenGL ES 上下文。
    // 我们要把窗口 HWND 给 DX12（步 04+）。
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    GLFWwindow* window = glfwCreateWindow(1280, 720,
                                          "WNanite — step 03",
                                          nullptr, nullptr);
    if (window == nullptr)
    {
        std::fprintf(stderr, "glfwCreateWindow failed\n");
        glfwTerminate();
        return 1;
    }
    glfwSetKeyCallback(window, on_glfw_key);

    // 主循环：纯泵事件。
    // 暂不渲染、不限速；CPU 单核占用可能接近 100%，本步不解决。
    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();
    }

    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
