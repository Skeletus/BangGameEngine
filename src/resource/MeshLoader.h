#pragma once

#include <bgfx/bgfx.h>
#include <cstddef>
#include <functional>
#include <string>
#include <vector>

#include "../asset/Mesh.h"
#include "../render/Material.h"

namespace resource
{
struct MeshLoadResult
{
    Mesh mesh;
    std::vector<Material> materials;
    uint32_t vertexCount = 0;
    size_t approxBytes = 0;
};

bool LoadMeshFromObj(const std::string& absolutePath,
                     const bgfx::VertexLayout& layout,
                     bgfx::TextureHandle fallbackTex,
                     MeshLoadResult& outResult,
                     std::string* outLog = nullptr,
                     const std::function<bgfx::TextureHandle(const std::string&)>& textureLoader = {});
}
