#pragma once

#include <bgfx/bgfx.h>
#include <cstddef>
#include <string>

namespace resource
{
struct TextureLoadResult
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    int width = 0;
    int height = 0;
    size_t approxBytes = 0;
};

TextureLoadResult LoadTextureFromFile(const std::string& absolutePath,
                                      bool generateMips = false,
                                      uint64_t flags = BGFX_TEXTURE_NONE);
}
