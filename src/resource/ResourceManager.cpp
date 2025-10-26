#include "ResourceManager.h"

#include <bgfx/bgfx.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>
#include <utility>

#ifdef _WIN32
#include <windows.h>
#endif

#include "MeshLoader.h"
#include "TextureLoader.h"

namespace resource
{
namespace
{
struct MeshDeleter
{
    void operator()(Mesh* mesh) const noexcept
    {
        if (mesh)
        {
            mesh->destroy();
            delete mesh;
        }
    }
};

struct MaterialDeleter
{
    void operator()(Material* material) const noexcept
    {
        if (material)
        {
            material->destroy();
            delete material;
        }
    }
};

static std::string ExeDir()
{
#ifdef _WIN32
    char path[MAX_PATH]{0};
    DWORD n = GetModuleFileNameA(nullptr, path, MAX_PATH);
    if (n == 0 || n == MAX_PATH)
    {
        return std::string{};
    }
    std::filesystem::path p(path);
    return p.parent_path().string();
#else
    return std::filesystem::current_path().string();
#endif
}

static std::string DetectAssetsBase()
{
    if (const char* env = std::getenv("SANDBOXCITY_ASSETS_DIR"))
    {
        std::filesystem::path base(env);
        if (std::filesystem::exists(base))
        {
            std::printf("[ASSETS] Usando SANDBOXCITY_ASSETS_DIR: %s\n", base.string().c_str());
            return base.string();
        }
        std::printf("[ASSETS] SANDBOXCITY_ASSETS_DIR no existe: %s\n", base.string().c_str());
    }
    {
        std::filesystem::path base = std::filesystem::path(ExeDir()) / "assets";
        if (std::filesystem::exists(base))
        {
            std::printf("[ASSETS] Usando carpeta junto al ejecutable: %s\n", base.string().c_str());
            return base.string();
        }
    }
    {
        std::filesystem::path base = std::filesystem::path(ExeDir()) / ".." / ".." / ".." / "assets";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base))
        {
            std::printf("[ASSETS] Usando fallback ../../../assets: %s\n", base.string().c_str());
            return base.string();
        }
    }
    {
        std::filesystem::path base = std::filesystem::path(ExeDir()) / ".." / ".." / "assets";
        base = std::filesystem::weakly_canonical(base);
        if (std::filesystem::exists(base))
        {
            std::printf("[ASSETS] Usando fallback ../../assets: %s\n", base.string().c_str());
            return base.string();
        }
    }
    std::printf("[ASSETS] ERROR: No se encontró carpeta 'assets'\n");
    return (std::filesystem::path(ExeDir()) / "assets").string();
}

static std::string CacheTypeName(ResourceManager::CacheType type)
{
    switch (type)
    {
    case ResourceManager::CacheType::Texture:  return "Texture";
    case ResourceManager::CacheType::Material: return "Material";
    case ResourceManager::CacheType::Mesh:     return "Mesh";
    }
    return "Unknown";
}
}

TextureResource::~TextureResource()
{
    if (bgfx::isValid(handle))
    {
        bgfx::destroy(handle);
        handle = bgfx::TextureHandle{bgfx::kInvalidHandle};
    }
}

ResourceManager::ResourceManager()
{
}

ResourceManager::~ResourceManager()
{
    Shutdown();
}

bool ResourceManager::Initialize()
{
    if (m_initialized)
    {
        return true;
    }

    m_layout.begin()
        .add(bgfx::Attrib::Position, 3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Normal,   3, bgfx::AttribType::Float)
        .add(bgfx::Attrib::Color0,   4, bgfx::AttribType::Uint8, true)
        .add(bgfx::Attrib::TexCoord0,2, bgfx::AttribType::Float)
    .end();
    m_vertexStride = m_layout.getStride();

    m_assetsRoot = DetectAssetsBase();
    m_assetsRoot = std::filesystem::weakly_canonical(m_assetsRoot).string();

    EnsureDefaultResources();

    m_initialized = true;
    return true;
}

void ResourceManager::Shutdown()
{
    m_meshCache.clear();
    m_materialCache.clear();
    m_textureCache.clear();
    m_defaultMaterial.reset();
    m_checkerTexture.reset();
    m_initialized = false;
}

std::shared_ptr<TextureResource> ResourceManager::LoadTexture(const std::string& relativePath)
{
    EnsureDefaultResources();
    const std::string normalized = NormalizePath(relativePath);
    if (normalized.empty())
    {
        return m_checkerTexture;
    }

    auto it = m_textureCache.find(normalized);
    if (it != m_textureCache.end())
    {
        LogCacheHit(CacheType::Texture, normalized);
        return it->second;
    }

    const std::string absolute = BuildAbsolutePath(normalized);
    if (!std::filesystem::exists(absolute))
    {
        std::printf("[TEX] No existe: %s\n", absolute.c_str());
        LogCacheMiss(CacheType::Texture, normalized);
        if (m_checkerTexture)
        {
            m_textureCache[normalized] = m_checkerTexture;
        }
        return m_checkerTexture;
    }

    return LoadTextureInternal(normalized, absolute);
}

std::shared_ptr<Material> ResourceManager::LoadMaterial(const std::string& relativePath)
{
    EnsureDefaultResources();
    const std::string normalized = NormalizePath(relativePath);
    if (normalized.empty())
    {
        return GetDefaultMaterial();
    }

    auto it = m_materialCache.find(normalized);
    if (it != m_materialCache.end())
    {
        LogCacheHit(CacheType::Material, normalized);
        return it->second->material;
    }

    const std::string absolute = BuildAbsolutePath(normalized);
    if (!std::filesystem::exists(absolute))
    {
        std::printf("[MTL] No existe: %s\n", absolute.c_str());
        LogCacheMiss(CacheType::Material, normalized);
        return GetDefaultMaterial();
    }

    std::ifstream file(absolute);
    if (!file)
    {
        std::printf("[MTL] No se pudo abrir: %s\n", absolute.c_str());
        LogCacheMiss(CacheType::Material, normalized);
        return GetDefaultMaterial();
    }

    Material materialData;
    materialData.reset();
    materialData.albedo = m_checkerTexture ? m_checkerTexture->handle : bgfx::TextureHandle{bgfx::kInvalidHandle};
    materialData.ownsTexture = false;

    std::string mapKd;
    std::string line;
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::string token;
        if (!(iss >> token))
        {
            continue;
        }
        if (token == "Kd")
        {
            iss >> materialData.baseTint[0] >> materialData.baseTint[1] >> materialData.baseTint[2];
            materialData.baseTint[3] = 1.0f;
        }
        else if (token == "map_Kd")
        {
            iss >> mapKd;
        }
    }

    std::shared_ptr<TextureResource> textureRef = m_checkerTexture;
    if (!mapKd.empty())
    {
        std::filesystem::path texAbs = std::filesystem::path(absolute).parent_path() / mapKd;
        std::error_code ec;
        std::filesystem::path texRel = std::filesystem::relative(texAbs, m_assetsRoot, ec);
        std::string normalizedTex = NormalizePath(ec ? texAbs.string() : texRel.generic_string());
        textureRef = LoadTextureInternal(normalizedTex, texAbs.lexically_normal().string());
        if (textureRef)
        {
            materialData.albedo = textureRef->handle;
        }
        else
        {
            textureRef = m_checkerTexture;
            materialData.albedo = m_checkerTexture ? m_checkerTexture->handle : bgfx::TextureHandle{bgfx::kInvalidHandle};
        }
    }

    auto matPtr = CreateMaterialFromData(materialData, normalized);
    auto entry = std::make_shared<MaterialEntry>();
    entry->material = matPtr;
    entry->albedoTexture = textureRef;
    entry->approxBytes = sizeof(Material);
    entry->source = normalized;
    m_materialCache[normalized] = entry;
    LogCacheMiss(CacheType::Material, normalized);
    return entry->material;
}

std::shared_ptr<MeshEntry> ResourceManager::LoadMesh(const std::string& relativePath)
{
    EnsureDefaultResources();
    const std::string normalized = NormalizePath(relativePath);
    if (normalized.empty())
    {
        return nullptr;
    }

    auto it = m_meshCache.find(normalized);
    if (it != m_meshCache.end())
    {
        LogCacheHit(CacheType::Mesh, normalized);
        return it->second;
    }

    const std::string absolute = BuildAbsolutePath(normalized);
    if (!std::filesystem::exists(absolute))
    {
        std::printf("[MESH] No existe: %s\n", absolute.c_str());
        LogCacheMiss(CacheType::Mesh, normalized);
        return nullptr;
    }

    MeshLoadResult result;
    const bgfx::TextureHandle fallback = m_checkerTexture ? m_checkerTexture->handle : bgfx::TextureHandle{bgfx::kInvalidHandle};
    auto textureLoader = [this](const std::string& absPath) -> bgfx::TextureHandle {
        std::filesystem::path abs(absPath);
        std::error_code ec;
        std::filesystem::path rel = std::filesystem::relative(abs, m_assetsRoot, ec);
        std::string normalizedRel = NormalizePath(ec ? abs.generic_string() : rel.generic_string());
        auto tex = LoadTextureInternal(normalizedRel, abs.lexically_normal().string());
        return tex ? tex->handle : (m_checkerTexture ? m_checkerTexture->handle : bgfx::TextureHandle{bgfx::kInvalidHandle});
    };

    std::string log;
    if (!LoadMeshFromObj(absolute, m_layout, fallback, result, &log, textureLoader))
    {
        if (!log.empty())
        {
            std::printf("[MESH] %s\n", log.c_str());
        }
        LogCacheMiss(CacheType::Mesh, normalized);
        return nullptr;
    }

    auto entry = std::make_shared<MeshEntry>();
    auto meshPtr = std::shared_ptr<Mesh>(new Mesh(), MeshDeleter{});
    *meshPtr = result.mesh;
    entry->mesh = meshPtr;
    entry->subsets = result.subsets;
    entry->approxBytes = result.approxBytes;
    entry->source = normalized;

    entry->materials.reserve(result.materials.size());
    for (const Material& mtl : result.materials)
    {
        entry->materials.push_back(CreateMaterialFromData(mtl, normalized));
    }

    m_meshCache[normalized] = entry;
    LogCacheMiss(CacheType::Mesh, normalized);
    return entry;
}

std::shared_ptr<Material> ResourceManager::GetDefaultMaterial() const
{
    if (!m_defaultMaterial)
    {
        return nullptr;
    }
    return m_defaultMaterial->material;
}

void ResourceManager::PrintStats() const
{
    size_t texMem = 0;
    for (const auto& [_, tex] : m_textureCache)
    {
        if (tex)
        {
            texMem += tex->approxBytes;
        }
    }
    size_t matMem = 0;
    for (const auto& [_, mat] : m_materialCache)
    {
        if (mat)
        {
            matMem += mat->approxBytes;
        }
    }
    size_t meshMem = 0;
    for (const auto& [_, mesh] : m_meshCache)
    {
        if (mesh)
        {
            meshMem += mesh->approxBytes;
        }
    }

    size_t meshMaterialCount = 0;
    for (const auto& [_, mesh] : m_meshCache)
    {
        if (mesh)
        {
            meshMaterialCount += mesh->materials.size();
        }
    }

    std::printf("[RES] ===== Resource Stats =====\n");
    std::printf("[RES] Textures: %zu | Approx GPU bytes: %zu | HITs: %zu | MISS: %zu\n",
                m_textureCache.size(), texMem, m_textureHits, m_textureMiss);
    std::printf("[RES] Materials: %zu (+%zu mesh-local) | Approx bytes: %zu | HITs: %zu | MISS: %zu\n",
                m_materialCache.size(), meshMaterialCount, matMem,
                m_materialHits, m_materialMiss);
    std::printf("[RES] Meshes: %zu | Approx GPU bytes: %zu | HITs: %zu | MISS: %zu\n",
                m_meshCache.size(), meshMem, m_meshHits, m_meshMiss);
}

bool ResourceManager::Reload(const std::string& relativePath)
{
    const std::string normalized = NormalizePath(relativePath);
    if (normalized.empty())
    {
        return false;
    }

    std::filesystem::path path(normalized);
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c){ return std::tolower(c); });

    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".dds")
    {
        m_textureCache.erase(normalized);
        LoadTexture(normalized);
        return true;
    }
    if (ext == ".mtl")
    {
        m_materialCache.erase(normalized);
        LoadMaterial(normalized);
        return true;
    }
    if (ext == ".obj")
    {
        m_meshCache.erase(normalized);
        LoadMesh(normalized);
        return true;
    }
    return false;
}

std::string ResourceManager::NormalizePath(const std::string& relativePath) const
{
    if (relativePath.empty())
    {
        return std::string{};
    }
    std::filesystem::path p(relativePath);
    if (p.is_absolute())
    {
        std::error_code ec;
        p = std::filesystem::relative(p, m_assetsRoot, ec);
        if (ec)
        {
            p = p.lexically_normal();
        }
    }
    std::string result = p.lexically_normal().generic_string();
    if (result.rfind("./", 0) == 0)
    {
        result.erase(0, 2);
    }
    return result;
}

std::string ResourceManager::BuildAbsolutePath(const std::string& normalizedRelative) const
{
    std::filesystem::path base(m_assetsRoot);
    std::filesystem::path rel(normalizedRelative);
    std::filesystem::path full = std::filesystem::weakly_canonical(base / rel);
    return full.string();
}

std::shared_ptr<TextureResource> ResourceManager::LoadTextureInternal(const std::string& normalizedRelative,
                                                                      const std::string& absolutePath,
                                                                      bool logHitMiss)
{
    if (normalizedRelative.empty())
    {
        return m_checkerTexture;
    }

    auto it = m_textureCache.find(normalizedRelative);
    if (it != m_textureCache.end())
    {
        if (logHitMiss)
        {
            LogCacheHit(CacheType::Texture, normalizedRelative);
        }
        return it->second;
    }

    TextureLoadResult data = LoadTextureFromFile(absolutePath);
    if (!bgfx::isValid(data.handle))
    {
        if (logHitMiss)
        {
            LogCacheMiss(CacheType::Texture, normalizedRelative);
        }
        if (m_checkerTexture)
        {
            m_textureCache[normalizedRelative] = m_checkerTexture;
        }
        return m_checkerTexture;
    }

    auto tex = std::make_shared<TextureResource>();
    tex->handle = data.handle;
    tex->width = data.width;
    tex->height = data.height;
    tex->approxBytes = data.approxBytes;
    tex->source = normalizedRelative;
    m_textureCache[normalizedRelative] = tex;
    if (logHitMiss)
    {
        LogCacheMiss(CacheType::Texture, normalizedRelative);
    }
    return tex;
}

std::shared_ptr<TextureResource> ResourceManager::CreateProceduralChecker()
{
    const uint8_t pix[] = {
        255,255,255,255,  64,64,64,255,
         64,64,64,255,   255,255,255,255
    };
    const bgfx::Memory* mem = bgfx::copy(pix, sizeof(pix));
    bgfx::TextureHandle handle = bgfx::createTexture2D(2, 2, false, 1, bgfx::TextureFormat::RGBA8, BGFX_TEXTURE_NONE, mem);

    auto tex = std::make_shared<TextureResource>();
    tex->handle = handle;
    tex->width = 2;
    tex->height = 2;
    tex->approxBytes = sizeof(pix);
    tex->source = "procedural_checker";
    return tex;
}

std::shared_ptr<Material> ResourceManager::CreateMaterialFromData(const Material& src,
                                                                  const std::string& sourcePath)
{
    auto ptr = std::shared_ptr<Material>(new Material(), MaterialDeleter{});
    *ptr = src;
    ptr->ownsTexture = false;
    return ptr;
}

std::shared_ptr<Material> ResourceManager::CreateDefaultMaterial()
{
    Material mat;
    mat.reset();
    mat.baseTint[0] = mat.baseTint[1] = mat.baseTint[2] = 1.0f;
    mat.baseTint[3] = 1.0f;
    mat.albedo = m_checkerTexture ? m_checkerTexture->handle : bgfx::TextureHandle{bgfx::kInvalidHandle};
    mat.ownsTexture = false;
    mat.specParams[0] = 32.0f;
    mat.specParams[1] = 0.35f;
    return CreateMaterialFromData(mat, "default");
}

void ResourceManager::EnsureDefaultResources()
{
    if (!m_checkerTexture)
    {
        m_checkerTexture = CreateProceduralChecker();
    }

    const std::string checkerRel = NormalizePath("textures/checker.png");
    if (m_textureCache.find(checkerRel) == m_textureCache.end())
    {
        m_textureCache[checkerRel] = m_checkerTexture;
    }

    std::filesystem::path checkerAbs(BuildAbsolutePath(checkerRel));
    if (std::filesystem::exists(checkerAbs))
    {
        auto tex = LoadTextureInternal(checkerRel, checkerAbs.string(), false);
        if (tex)
        {
            m_checkerTexture = tex;
        }
    }

    if (!m_defaultMaterial)
    {
        auto matPtr = CreateDefaultMaterial();
        auto entry = std::make_shared<MaterialEntry>();
        entry->material = matPtr;
        entry->albedoTexture = m_checkerTexture;
        entry->approxBytes = sizeof(Material);
        entry->source = "default";
        m_defaultMaterial = entry;
        m_materialCache["__default__"] = entry;
    }
}

void ResourceManager::LogCacheHit(CacheType type, const std::string& path) const
{
    switch (type)
    {
    case CacheType::Texture:  ++m_textureHits; break;
    case CacheType::Material: ++m_materialHits; break;
    case CacheType::Mesh:     ++m_meshHits; break;
    }
    std::printf("[RES] %s cache HIT: %s\n", CacheTypeName(type), path.c_str());
}

void ResourceManager::LogCacheMiss(CacheType type, const std::string& path) const
{
    switch (type)
    {
    case CacheType::Texture:  ++m_textureMiss; break;
    case CacheType::Material: ++m_materialMiss; break;
    case CacheType::Mesh:     ++m_meshMiss; break;
    }
    std::printf("[RES] %s cache MISS: %s\n", CacheTypeName(type), path.c_str());
}
}
