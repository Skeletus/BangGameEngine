#include "SceneLoader.h"

#include "../ecs/Scene.h"
#include "../ecs/Transform.h"
#include "../ecs/MeshRenderer.h"
#include "../resource/ResourceManager.h"
#include "../asset/Mesh.h"
#include "../render/Material.h"

#include <nlohmann/json.hpp>
#include <bx/math.h>

#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <vector>
#include <cstdio>
#include <cstdlib>

using json = nlohmann::json;

namespace
{
struct LoadContext
{
    Scene& scene;
    resource::ResourceManager& resources;
    std::unordered_map<std::string, std::shared_ptr<resource::TextureResource>> textures;
    std::unordered_map<std::string, std::shared_ptr<Material>> materials;
    std::unordered_map<std::string, std::shared_ptr<resource::MeshEntry>> meshes;
    std::unordered_map<std::string, EntityId> entityLookup;
    std::vector<std::pair<EntityId, std::string>> pendingParentRefs;
    size_t autoNameCounter = 0;
};

static std::filesystem::path StripAssetsPrefix(const std::filesystem::path& in)
{
    if (in.empty())
    {
        return in;
    }

    auto it = in.begin();
    if (it != in.end() && it->string() == "assets")
    {
        std::filesystem::path stripped;
        ++it;
        for (; it != in.end(); ++it)
        {
            stripped /= *it;
        }
        return stripped;
    }
    return in;
}

static std::filesystem::path ResolveScenePath(const std::string& requested,
                                              resource::ResourceManager& resources)
{
    std::filesystem::path path(requested);
    std::error_code ec;

    if (path.is_absolute())
    {
        if (std::filesystem::exists(path))
        {
            return std::filesystem::weakly_canonical(path, ec);
        }
        return {};
    }

    std::filesystem::path cwd = std::filesystem::current_path() / path;
    if (std::filesystem::exists(cwd))
    {
        return std::filesystem::weakly_canonical(cwd, ec);
    }

    std::filesystem::path assetsRoot(resources.GetAssetsRoot());
    if (!assetsRoot.empty())
    {
        std::filesystem::path candidate = assetsRoot / path;
        if (std::filesystem::exists(candidate))
        {
            return std::filesystem::weakly_canonical(candidate, ec);
        }

        std::filesystem::path stripped = assetsRoot / StripAssetsPrefix(path);
        if (std::filesystem::exists(stripped))
        {
            return std::filesystem::weakly_canonical(stripped, ec);
        }
    }

    return {};
}

static void RegisterEntityKey(LoadContext& ctx, EntityId entity, const std::string& key)
{
    if (key.empty())
    {
        return;
    }

    auto [it, inserted] = ctx.entityLookup.emplace(key, entity);
    if (!inserted)
    {
        std::printf("[SceneLoader] Aviso: identificador de entidad duplicado '%s', sobreescribiendo.\n", key.c_str());
        it->second = entity;
    }
}

static void LoadTexturesFromJson(const json& texturesJson, LoadContext& ctx)
{
    for (auto it = texturesJson.begin(); it != texturesJson.end(); ++it)
    {
        const std::string texId = it.key();
        if (!it.value().is_string())
        {
            std::printf("[SceneLoader] Textura '%s' inválida: se esperaba una ruta en string.\n", texId.c_str());
            continue;
        }
        const std::string relPath = it.value().get<std::string>();
        auto tex = ctx.resources.LoadTexture(relPath);
        if (!tex)
        {
            std::printf("[SceneLoader] No se pudo cargar textura '%s' (%s), usando checker.\n",
                        texId.c_str(), relPath.c_str());
            tex = ctx.resources.GetCheckerTexture();
        }
        ctx.textures[texId] = tex;
    }
}

static void LoadMaterialsFromJson(const json& materialsJson, LoadContext& ctx)
{
    for (auto it = materialsJson.begin(); it != materialsJson.end(); ++it)
    {
        const std::string matId = it.key();
        if (!it.value().is_object())
        {
            std::printf("[SceneLoader] Material '%s' inválido: se esperaba un objeto.\n", matId.c_str());
            continue;
        }

        const json& matJson = it.value();
        auto material = std::make_shared<Material>();
        material->reset();
        material->ownsTexture = false;

        if (auto tintIt = matJson.find("baseTint"); tintIt != matJson.end() && tintIt->is_array())
        {
            for (size_t i = 0; i < 4 && i < tintIt->size(); ++i)
            {
                if ((*tintIt)[i].is_number_float() || (*tintIt)[i].is_number_integer())
                {
                    material->baseTint[i] = (*tintIt)[i].get<float>();
                }
            }
        }

        if (auto uvIt = matJson.find("uv"); uvIt != matJson.end() && uvIt->is_array())
        {
            for (size_t i = 0; i < 2 && i < uvIt->size(); ++i)
            {
                if ((*uvIt)[i].is_number_float() || (*uvIt)[i].is_number_integer())
                {
                    material->uvScale[i] = (*uvIt)[i].get<float>();
                }
            }
        }

        std::shared_ptr<resource::TextureResource> texResource;
        if (auto texIt = matJson.find("albedoTex"); texIt != matJson.end() && texIt->is_string())
        {
            const std::string texId = texIt->get<std::string>();
            auto lookup = ctx.textures.find(texId);
            if (lookup != ctx.textures.end())
            {
                texResource = lookup->second;
            }
            else
            {
                std::printf("[SceneLoader] Textura '%s' no encontrada para material '%s', usando checker.\n",
                            texId.c_str(), matId.c_str());
            }
        }

        if (!texResource)
        {
            texResource = ctx.resources.GetCheckerTexture();
        }

        if (texResource && bgfx::isValid(texResource->handle))
        {
            material->albedo = texResource->handle;
        }
        else
        {
            material->albedo = bgfx::TextureHandle{bgfx::kInvalidHandle};
        }

        ctx.materials[matId] = material;
    }
}

static void LoadMeshesFromJson(const json& meshesJson, LoadContext& ctx)
{
    for (auto it = meshesJson.begin(); it != meshesJson.end(); ++it)
    {
        const std::string meshId = it.key();
        if (!it.value().is_object())
        {
            std::printf("[SceneLoader] Malla '%s' inválida: se esperaba un objeto.\n", meshId.c_str());
            continue;
        }

        const json& meshJson = it.value();
        const std::string objPath = meshJson.value("obj", std::string{});
        if (objPath.empty())
        {
            std::printf("[SceneLoader] Malla '%s' sin ruta OBJ.\n", meshId.c_str());
            continue;
        }

        auto meshEntry = ctx.resources.LoadMesh(objPath);
        if (!meshEntry)
        {
            std::printf("[SceneLoader] Fallo al cargar OBJ '%s' para malla '%s'.\n",
                        objPath.c_str(), meshId.c_str());
            continue;
        }

        ctx.meshes[meshId] = meshEntry;

        const std::string mtlPath = meshJson.value("mtl", std::string{});
        if (!mtlPath.empty())
        {
            ctx.resources.LoadMaterial(mtlPath);
        }
    }
}

static void ApplyTransformFromJson(const json& transformJson, Transform& transform)
{
    auto readVec3 = [](const json& parent, const char* key, const float defaults[3], float out[3])
    {
        out[0] = defaults[0];
        out[1] = defaults[1];
        out[2] = defaults[2];
        auto it = parent.find(key);
        if (it == parent.end() || !it->is_array())
        {
            return false;
        }

        bool modified = false;
        for (size_t i = 0; i < 3 && i < it->size(); ++i)
        {
            if ((*it)[i].is_number_float() || (*it)[i].is_number_integer())
            {
                out[i] = (*it)[i].get<float>();
                modified = true;
            }
        }
        return modified;
    };

    const float defaultPos[3] = {transform.position.x, transform.position.y, transform.position.z};
    const float defaultRot[3] = {transform.rotationEuler.x, transform.rotationEuler.y, transform.rotationEuler.z};
    const float defaultScale[3] = {transform.scale.x, transform.scale.y, transform.scale.z};

    float pos[3];
    if (readVec3(transformJson, "position", defaultPos, pos))
    {
        transform.position.x = pos[0];
        transform.position.y = pos[1];
        transform.position.z = pos[2];
    }

    bool hasRotation = false;
    float rot[3];
    if (readVec3(transformJson, "rotationEuler", defaultRot, rot))
    {
        hasRotation = true;
    }

    float rotDeg[3];
    if (readVec3(transformJson, "rotationEulerDeg", defaultRot, rotDeg))
    {
        rot[0] = bx::toRad(rotDeg[0]);
        rot[1] = bx::toRad(rotDeg[1]);
        rot[2] = bx::toRad(rotDeg[2]);
        hasRotation = true;
    }

    if (hasRotation)
    {
        transform.rotationEuler.x = rot[0];
        transform.rotationEuler.y = rot[1];
        transform.rotationEuler.z = rot[2];
    }

    float scl[3];
    if (readVec3(transformJson, "scale", defaultScale, scl))
    {
        transform.scale.x = scl[0];
        transform.scale.y = scl[1];
        transform.scale.z = scl[2];
    }

    transform.MarkDirty();
}

static void ApplyMeshRendererFromJson(const json& mrJson,
                                      LoadContext& ctx,
                                      EntityId entity,
                                      const std::string& entityLabel)
{
    if (!mrJson.is_object())
    {
        return;
    }

    const std::string meshId = mrJson.value("mesh", std::string{});
    if (meshId.empty())
    {
        std::printf("[SceneLoader] Entidad '%s' sin 'mesh'.\n", entityLabel.c_str());
        return;
    }

    auto meshIt = ctx.meshes.find(meshId);
    if (meshIt == ctx.meshes.end() || !meshIt->second || !meshIt->second->mesh)
    {
        std::printf("[SceneLoader] Malla '%s' no encontrada para entidad '%s'.\n",
                    meshId.c_str(), entityLabel.c_str());
        return;
    }

    MeshRenderer* renderer = ctx.scene.AddMeshRenderer(entity);
    if (!renderer)
    {
        return;
    }

    renderer->mesh = meshIt->second->mesh;
    renderer->material = ctx.resources.GetDefaultMaterial();
    renderer->materialOverrides.clear();

    if (auto overridesIt = mrJson.find("materialOverrides"); overridesIt != mrJson.end() && overridesIt->is_object())
    {
        for (auto matIt = overridesIt->begin(); matIt != overridesIt->end(); ++matIt)
        {
            const std::string submeshKey = matIt.key();
            if (!matIt.value().is_string())
            {
                continue;
            }

            uint32_t submeshIndex = 0;
            try
            {
                submeshIndex = static_cast<uint32_t>(std::stoul(submeshKey));
            }
            catch (...)
            {
                std::printf("[SceneLoader] Índice de submesh '%s' inválido en entidad '%s'.\n",
                            submeshKey.c_str(), entityLabel.c_str());
                continue;
            }

            const std::string materialId = matIt.value().get<std::string>();
            auto materialLookup = ctx.materials.find(materialId);
            std::shared_ptr<Material> materialPtr;
            if (materialLookup != ctx.materials.end())
            {
                materialPtr = materialLookup->second;
            }
            else
            {
                std::printf("[SceneLoader] Material '%s' no encontrado para override en entidad '%s'.\n",
                            materialId.c_str(), entityLabel.c_str());
                materialPtr = ctx.resources.GetDefaultMaterial();
            }

            if (materialPtr)
            {
                renderer->materialOverrides[submeshIndex] = materialPtr;
            }
        }
    }
}

static void ProcessEntityJson(const json& entityJson,
                              LoadContext& ctx,
                              EntityId forcedParent)
{
    EntityId entity = ctx.scene.CreateEntity();
    const std::string name = entityJson.value("name", std::string{});
    const std::string explicitId = entityJson.value("id", std::string{});

    std::string label = !name.empty() ? name : (!explicitId.empty() ? explicitId : ("Entity" + std::to_string(entity)));
    RegisterEntityKey(ctx, entity, name);
    RegisterEntityKey(ctx, entity, explicitId);

    if (name.empty() && explicitId.empty())
    {
        std::string autoKey = "__entity_" + std::to_string(ctx.autoNameCounter++);
        RegisterEntityKey(ctx, entity, autoKey);
    }

    Transform* transform = ctx.scene.AddTransform(entity);
    if (transform)
    {
        ApplyTransformFromJson(entityJson.value("transform", json::object()), *transform);
    }

    if (auto mrIt = entityJson.find("meshRenderer"); mrIt != entityJson.end())
    {
        ApplyMeshRendererFromJson(*mrIt, ctx, entity, label);
    }

    if (auto parentIt = entityJson.find("parent"); parentIt != entityJson.end() && parentIt->is_string())
    {
        ctx.pendingParentRefs.emplace_back(entity, parentIt->get<std::string>());
    }
    else if (forcedParent != kInvalidEntity)
    {
        ctx.scene.SetParent(entity, forcedParent);
    }

    if (auto childrenIt = entityJson.find("children"); childrenIt != entityJson.end() && childrenIt->is_array())
    {
        for (const auto& childJson : *childrenIt)
        {
            if (childJson.is_object())
            {
                ProcessEntityJson(childJson, ctx, entity);
            }
        }
    }
}

} // namespace

bool LoadSceneFromJson(const std::string& path,
                       Scene& scene,
                       resource::ResourceManager& resources,
                       std::string* err)
{
    std::filesystem::path resolved = ResolveScenePath(path, resources);
    if (resolved.empty())
    {
        const std::string message = "No se encontró el archivo de escena: " + path;
        if (err) *err = message;
        std::printf("[SceneLoader] %s\n", message.c_str());
        return false;
    }

    std::ifstream file(resolved);
    if (!file)
    {
        const std::string message = "No se pudo abrir la escena: " + resolved.string();
        if (err) *err = message;
        std::printf("[SceneLoader] %s\n", message.c_str());
        return false;
    }

    json data;
    try
    {
        file >> data;
    }
    catch (const json::parse_error& e)
    {
        const std::string message = std::string("Error al parsear JSON: ") + e.what();
        if (err) *err = message;
        std::printf("[SceneLoader] %s\n", message.c_str());
        return false;
    }

    Scene newScene;
    LoadContext ctx{newScene, resources};

    if (auto resIt = data.find("resources"); resIt != data.end() && resIt->is_object())
    {
        if (auto texIt = resIt->find("textures"); texIt != resIt->end() && texIt->is_object())
        {
            LoadTexturesFromJson(*texIt, ctx);
        }
        if (auto matIt = resIt->find("materials"); matIt != resIt->end() && matIt->is_object())
        {
            LoadMaterialsFromJson(*matIt, ctx);
        }
        if (auto meshIt = resIt->find("meshes"); meshIt != resIt->end() && meshIt->is_object())
        {
            LoadMeshesFromJson(*meshIt, ctx);
        }
    }

    if (auto entitiesIt = data.find("entities"); entitiesIt != data.end())
    {
        if (!entitiesIt->is_array())
        {
            const std::string message = "El campo 'entities' debe ser un arreglo.";
            if (err) *err = message;
            std::printf("[SceneLoader] %s\n", message.c_str());
            return false;
        }

        for (const auto& entityJson : *entitiesIt)
        {
            if (!entityJson.is_object())
            {
                continue;
            }
            ProcessEntityJson(entityJson, ctx, kInvalidEntity);
        }
    }

    for (const auto& [child, parentKey] : ctx.pendingParentRefs)
    {
        auto it = ctx.entityLookup.find(parentKey);
        if (it != ctx.entityLookup.end())
        {
            ctx.scene.SetParent(child, it->second);
        }
        else
        {
            std::printf("[SceneLoader] No se encontró entidad padre '%s'.\n", parentKey.c_str());
        }
    }

    ctx.scene.SetLogicalLookup(std::move(ctx.entityLookup));

    scene = std::move(newScene);
    std::printf("[SceneLoader] Escena cargada desde %s\n", resolved.string().c_str());
    return true;
}

