// WNanite — 主入口
// 当前阶段：步 02 — 引入 GLFW（FetchContent），验证链接 + 运行时可调。
// 仍不开窗口（步 03）。

#include <GLFW/glfw3.h>
#include <cstdio>

int main()
{
    std::printf("hello, WNanite!\n");

    // 仅验证 GLFW 链接通且运行时能调；不创建任何窗口。
    if (!glfwInit())
    {
        std::printf("glfwInit failed\n");
        return 1;
    }
    std::printf("GLFW %s\n", glfwGetVersionString());
    glfwTerminate();

    return 0;
}
