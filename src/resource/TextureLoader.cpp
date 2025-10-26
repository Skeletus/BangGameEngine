#include "TextureLoader.h"

#include "../render/Texture.h"

namespace resource
{
TextureLoadResult LoadTextureFromFile(const std::string& absolutePath,
                                      bool generateMips,
                                      uint64_t flags)
{
    TextureLoadResult result;
    int width = 0;
    int height = 0;
    bgfx::TextureHandle handle = tex::LoadTexture2D(absolutePath.c_str(), generateMips, flags, &width, &height);
    result.handle = handle;
    result.width = width;
    result.height = height;
    if (bgfx::isValid(handle))
    {
        result.approxBytes = static_cast<size_t>(width) * static_cast<size_t>(height) * 4u;
    }
    return result;
}
}
