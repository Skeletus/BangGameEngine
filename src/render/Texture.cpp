#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

#include "Texture.h"
#include <bgfx/bgfx.h>
#include <cstdio>

namespace tex
{
    bgfx::TextureHandle LoadTexture2D(const char* path, bool genMips, uint64_t flags, int* outW, int* outH)
    {
        int w=0, h=0, comp=0;
        stbi_uc* data = stbi_load(path, &w, &h, &comp, 4); // forzamos RGBA8
        if (!data)
        {
            std::printf("[TEX] No se pudo cargar: %s\n", path);
            return BGFX_INVALID_HANDLE;
        }

        const uint32_t sizeBytes = (uint32_t)(w * h * 4);
        const bgfx::Memory* mem = bgfx::copy(data, sizeBytes);
        stbi_image_free(data);

        bgfx::TextureHandle th = bgfx::createTexture2D(
            (uint16_t)w, (uint16_t)h, genMips, 1, bgfx::TextureFormat::RGBA8, flags, mem);

        if (outW) *outW = w;
        if (outH) *outH = h;

        if (!bgfx::isValid(th))
            std::printf("[TEX] ERROR creando textura: %s\n", path);
        else
            std::printf("[TEX] OK %s (%dx%d)\n", path, w, h);

        return th;
    }
}
