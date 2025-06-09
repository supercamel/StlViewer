#include "mesh_loader.h"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include "shader_utils.h"
#include <stdexcept>

bgfx::VertexLayout Vertex::layout() {
    bgfx::VertexLayout layout;
    layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .end();
    return layout;
}

Mesh loadMesh(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate | aiProcess_GenNormals);
    if (!scene || !scene->HasMeshes())
        throw std::runtime_error("Failed to load mesh: " + path);

    const aiMesh* aMesh = scene->mMeshes[0];
    std::vector<Vertex> vertices(aMesh->mNumVertices);
    std::vector<uint16_t> indices;

    for (unsigned i = 0; i < aMesh->mNumVertices; ++i) {
        vertices[i].pos[0] = aMesh->mVertices[i].x;
        vertices[i].pos[1] = aMesh->mVertices[i].y;
        vertices[i].pos[2] = aMesh->mVertices[i].z;
    }

    for (unsigned i = 0; i < aMesh->mNumFaces; ++i) {
        const aiFace& face = aMesh->mFaces[i];
        if (face.mNumIndices == 3) {
            indices.push_back(static_cast<uint16_t>(face.mIndices[0]));
            indices.push_back(static_cast<uint16_t>(face.mIndices[1]));
            indices.push_back(static_cast<uint16_t>(face.mIndices[2]));
        }
    }

    bgfx::VertexBufferHandle vbo = bgfx::createVertexBuffer(
        bgfx::makeRef(vertices.data(), vertices.size() * sizeof(Vertex)),
        Vertex::layout()
    );

    bgfx::IndexBufferHandle ibo = bgfx::createIndexBuffer(
        bgfx::makeRef(indices.data(), indices.size() * sizeof(uint16_t))
    );

    Mesh mesh;
    mesh.vbo = vbo;
    mesh.ibo = ibo;
    mesh.indexCount = static_cast<uint32_t>(indices.size());
    mesh.program = loadProgram("vs_mesh", "fs_mesh");

    return mesh;
}

void Mesh::submit(bgfx::ViewId view) {
    bgfx::setVertexBuffer(0, vbo);
    bgfx::setIndexBuffer(ibo);
    bgfx::submit(view, program);
}
