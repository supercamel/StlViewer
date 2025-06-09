#include <bgfx/bgfx.h>
#include <bgfx/platform.h>
#include <GLFW/glfw3.h>
#include <iostream>
#include "mesh_loader.h"
#include "camera.h"
#include "shader_utils.h"

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: stlviewer <model.stl>\n";
        return 1;
    }

    glfwInit();
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    GLFWwindow* window = glfwCreateWindow(1280, 720, "STL Viewer", nullptr, nullptr);

    bgfx::PlatformData pd{};
#if defined(__linux__)
    pd.ndt = glfwGetX11Display();
    pd.nwh = (void*)(uintptr_t)glfwGetX11Window(window);
#elif defined(_WIN32)
    pd.nwh = glfwGetWin32Window(window);
#endif

    bgfx::Init init;
    init.type = bgfx::RendererType::OpenGL;
    init.resolution.width = 1280;
    init.resolution.height = 720;
    init.resolution.reset = BGFX_RESET_VSYNC;
    init.platformData = pd;
    bgfx::init(init);

    bgfx::setDebug(BGFX_DEBUG_TEXT);
    bgfx::setViewClear(0, BGFX_CLEAR_COLOR | BGFX_CLEAR_DEPTH, 0x303030ff, 1.0f, 0);
    bgfx::setViewRect(0, 0, 0, 1280, 720);

    Camera cam;
    Mesh mesh = loadMesh(argv[1]);

    while (!glfwWindowShouldClose(window)) {
        glfwPollEvents();
        cam.update(window);

        float view[16], proj[16];
        cam.getView(view);
        cam.getProj(proj, 1280.0f / 720.0f);
        bgfx::setViewTransform(0, view, proj);

        bgfx::touch(0);
        mesh.submit(0);
        bgfx::frame();
    }

    bgfx::shutdown();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
