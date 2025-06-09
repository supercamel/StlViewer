#include "shader_utils.h"
#include <fstream>
#include <vector>
#include <stdexcept>
#include <iostream>

static bgfx::ShaderHandle loadBinaryShader(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file)
        throw std::runtime_error("Failed to open shader: " + path);

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size))
        throw std::runtime_error("Failed to read shader: " + path);

    const bgfx::Memory* mem = bgfx::copy(buffer.data(), static_cast<uint32_t>(size));
    return bgfx::createShader(mem);
}

bgfx::ShaderHandle loadShader(const std::string& name) {
#if defined(BX_PLATFORM_LINUX)
    std::string base = "shaders/glsl/";
#elif defined(_WIN32)
    std::string base = "shaders/dx11/";
#else
    std::string base = "shaders/metal/";
#endif
    return loadBinaryShader(base + name + ".bin");
}

bgfx::ProgramHandle loadProgram(const std::string& vsName, const std::string& fsName) {
    bgfx::ShaderHandle vsh = loadShader(vsName);
    bgfx::ShaderHandle fsh = loadShader(fsName);
    return bgfx::createProgram(vsh, fsh, true); // auto destroy shaders
}
