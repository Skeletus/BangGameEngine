#include "MeshLoader.h"

#include <cstdio>
#include <utility>

#include "../asset/ObjLoader.h"

namespace resource
{
bool LoadMeshFromObj(const std::string& absolutePath,
                     const bgfx::VertexLayout& layout,
                     bgfx::TextureHandle fallbackTex,
                     MeshLoadResult& outResult,
                     std::string* outLog,
                     const std::function<bgfx::TextureHandle(const std::string&)>& textureLoader)
{
    outResult = MeshLoadResult{};

    std::vector<Material> materials;
    Mesh mesh;
    uint32_t vertexCount = 0;
    std::string log;
    bool ok = asset::LoadObjToMesh(absolutePath, layout, fallbackTex, mesh, materials,
                                   outLog ? outLog : &log, /*flipV=*/true, &vertexCount, textureLoader);
    if (!ok)
    {
        if (!log.empty())
        {
            std::printf("[MESH] %s\n", log.c_str());
        }
        return false;
    }

    outResult.mesh = std::move(mesh);
    outResult.materials = std::move(materials);
    outResult.vertexCount = vertexCount != 0 ? vertexCount : outResult.mesh.vertexCount;

    const uint32_t stride = layout.getStride();
    const uint32_t indices = outResult.mesh.indexCount;
    const uint32_t verts = outResult.vertexCount;
    outResult.approxBytes = static_cast<size_t>(verts) * stride + static_cast<size_t>(indices) * sizeof(uint16_t);
    return true;
}
}
