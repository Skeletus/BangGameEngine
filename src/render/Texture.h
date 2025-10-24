#pragma once
#include <bgfx/bgfx.h>
#include <cstdint>
#include <string>

namespace tex
{
    // Crea una textura 2D y devuelve su handle (BGFX_INVALID_HANDLE si falla)
    // flags por defecto: UV repeat + min/mag/mip LINEAR
    bgfx::TextureHandle LoadTexture2D(const char* path,
                                      bool genMips = true,
                                      uint64_t flags =
                                          BGFX_SAMPLER_U_REPEAT |
                                          BGFX_SAMPLER_V_REPEAT |
                                          BGFX_SAMPLER_W_REPEAT |
                                          BGFX_SAMPLER_MIN_LINEAR |
                                          BGFX_SAMPLER_MAG_LINEAR |
                                          BGFX_SAMPLER_MIP_LINEAR,
                                      int* outW = nullptr,
                                      int* outH = nullptr);

    inline bgfx::TextureHandle LoadTexture2D(const std::string& path,
                                             bool genMips = true,
                                             uint64_t flags =
                                                 BGFX_SAMPLER_U_REPEAT |
                                                 BGFX_SAMPLER_V_REPEAT |
                                                 BGFX_SAMPLER_W_REPEAT |
                                                 BGFX_SAMPLER_MIN_LINEAR |
                                                 BGFX_SAMPLER_MAG_LINEAR |
                                                 BGFX_SAMPLER_MIP_LINEAR,
                                             int* outW = nullptr,
                                             int* outH = nullptr)
    {
        return LoadTexture2D(path.c_str(), genMips, flags, outW, outH);
    }
}
