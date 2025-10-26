#pragma once

#include <bgfx/bgfx.h>

#include <cstddef>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

#include "../asset/Mesh.h"
#include "../render/Material.h"

namespace resource
{
struct TextureResource
{
    bgfx::TextureHandle handle = BGFX_INVALID_HANDLE;
    int width = 0;
    int height = 0;
    size_t approxBytes = 0;
    std::string source;

    ~TextureResource();
};

struct MaterialEntry
{
    std::shared_ptr<Material> material;
    std::shared_ptr<TextureResource> albedoTexture;
    size_t approxBytes = 0;
    std::string source;
};

struct MeshEntry
{
    std::shared_ptr<Mesh> mesh;
    std::vector<MeshSubset> subsets;
    std::vector<std::shared_ptr<Material>> materials;
    size_t approxBytes = 0;
    std::string source;
};

class ResourceManager
{
public:
    ResourceManager();
    ~ResourceManager();

    bool Initialize();
    void Shutdown();

    std::shared_ptr<TextureResource> LoadTexture(const std::string& relativePath);
    std::shared_ptr<Material> LoadMaterial(const std::string& relativePath);
    std::shared_ptr<MeshEntry> LoadMesh(const std::string& relativePath);

    std::shared_ptr<TextureResource> GetCheckerTexture() const { return m_checkerTexture; }
    std::shared_ptr<Material> GetDefaultMaterial() const;

    void PrintStats() const;
    bool Reload(const std::string& relativePath);

    const std::string& GetAssetsRoot() const { return m_assetsRoot; }

    enum class CacheType { Texture, Material, Mesh };

private:
    struct MeshCacheEntry;

    std::string NormalizePath(const std::string& relativePath) const;
    std::string BuildAbsolutePath(const std::string& normalizedRelative) const;

    std::shared_ptr<TextureResource> LoadTextureInternal(const std::string& normalizedRelative,
                                                         const std::string& absolutePath,
                                                         bool logHitMiss = true);
    std::shared_ptr<TextureResource> CreateProceduralChecker();
    std::shared_ptr<Material> CreateMaterialFromData(const Material& src,
                                                     const std::string& sourcePath);
    std::shared_ptr<Material> CreateDefaultMaterial();
    void EnsureDefaultResources();

    
    void LogCacheHit(CacheType type, const std::string& path) const;
    void LogCacheMiss(CacheType type, const std::string& path) const;

private:
    std::string m_assetsRoot;
    bgfx::VertexLayout m_layout{};
    uint32_t m_vertexStride = 0;

    std::unordered_map<std::string, std::shared_ptr<TextureResource>> m_textureCache;
    std::unordered_map<std::string, std::shared_ptr<MaterialEntry>>  m_materialCache;
    std::unordered_map<std::string, std::shared_ptr<MeshEntry>>       m_meshCache;

    std::shared_ptr<TextureResource> m_checkerTexture;
    std::shared_ptr<MaterialEntry>   m_defaultMaterial;

    mutable size_t m_textureHits = 0;
    mutable size_t m_textureMiss = 0;
    mutable size_t m_materialHits = 0;
    mutable size_t m_materialMiss = 0;
    mutable size_t m_meshHits = 0;
    mutable size_t m_meshMiss = 0;

    bool m_initialized = false;
};
}
