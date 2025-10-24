#pragma once
#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>

namespace tex
{
    // Crea una textura 2D y devuelve su handle (BGFX_INVALID_HANDLE si falla).
    // Nota: En tu bgfx (vcpkg) usa flags BGFX_TEXTURE_* (no BGFX_SAMPLER_*).
    //       El filtrado lineal y WRAP suelen ser por defecto => BGFX_TEXTURE_NONE.
    bgfx::TextureHandle LoadTexture2D(const char* path,
                                      bool hasMips = false,
                                      uint64_t flags = BGFX_TEXTURE_NONE,
                                      int* outW = nullptr,
                                      int* outH = nullptr);

    inline bgfx::TextureHandle LoadTexture2D(const std::string& path,
                                             bool genMips = true,
                                             uint64_t flags = BGFX_TEXTURE_NONE,
                                             int* outW = nullptr,
                                             int* outH = nullptr)
    {
        return LoadTexture2D(path.c_str(), genMips, flags, outW, outH);
    }
}
