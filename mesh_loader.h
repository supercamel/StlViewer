#ifndef MESH_LOADER_H
#define MESH_LOADER_H

#include <bgfx/bgfx.h>
#include <vector>
#include <string>

struct Vertex {
    float pos[3];
    static bgfx::VertexLayout layout();
};

struct Mesh {
    bgfx::VertexBufferHandle vbo;
    bgfx::IndexBufferHandle ibo;
    bgfx::ProgramHandle program;
    uint32_t indexCount;

    void submit(bgfx::ViewId view);
};

Mesh loadMesh(const std::string& path);

#endif // MESH_LOADER_H