#ifndef SHADER_UTILS_H
#define SHADER_UTILS_H

#include <bgfx/bgfx.h>
#include <string>

bgfx::ShaderHandle loadShader(const std::string& name);
bgfx::ProgramHandle loadProgram(const std::string& vsName, const std::string& fsName);

#endif // SHADER_UTILS_H