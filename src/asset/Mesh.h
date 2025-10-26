#pragma once
#include <bgfx/bgfx.h>
#include <cstdint>

struct MeshSubset
{
    uint32_t startIndex = 0;
    uint32_t indexCount = 0;
    int      materialIndex = -1;
};

struct Mesh {
    bgfx::VertexBufferHandle vbh = BGFX_INVALID_HANDLE;
    bgfx::IndexBufferHandle  ibh = BGFX_INVALID_HANDLE;
    uint32_t indexCount = 0;
    uint32_t vertexCount = 0;

    inline bool valid() const {
        return bgfx::isValid(vbh) && bgfx::isValid(ibh) && indexCount > 0;
    }
    inline void destroy() {
        if (bgfx::isValid(vbh)) bgfx::destroy(vbh);
        if (bgfx::isValid(ibh)) bgfx::destroy(ibh);
        vbh = BGFX_INVALID_HANDLE;
        ibh = BGFX_INVALID_HANDLE;
        indexCount = 0;
        vertexCount = 0;
    }
};
