#include "Scene.h"

#include "../asset/Mesh.h"
#include "../render/Material.h"

#include <algorithm>
#include <cstdio>

namespace
{
    constexpr size_t kTransformBit    = 0;
    constexpr size_t kMeshRendererBit = 1;

    const std::vector<EntityId> kEmptyChildren{};
}

EntityId Scene::CreateEntity()
{
    EntityId id = kInvalidEntity;
    if (!m_freeIds.empty())
    {
        id = m_freeIds.back();
        m_freeIds.pop_back();
    }
    else
    {
        id = ++m_nextId;
        if (id == kInvalidEntity)
        {
            id = ++m_nextId;
        }
    }

    m_entityMasks[id] = ComponentMask{};
    m_children[id];
    return id;
}

void Scene::DestroyEntity(EntityId id)
{
    if (!IsAlive(id))
    {
        return;
    }

    RemoveTransform(id);
    RemoveMeshRenderer(id);

    if (EntityId parent = GetParent(id); parent != kInvalidEntity)
    {
        auto it = m_children.find(parent);
        if (it != m_children.end())
        {
            auto& siblings = it->second;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), id), siblings.end());
        }
    }

    auto childIt = m_children.find(id);
    if (childIt != m_children.end())
    {
        for (EntityId child : childIt->second)
        {
            m_parents.erase(child);
            MarkHierarchyDirty(child);
        }
        m_children.erase(childIt);
    }

    m_parents.erase(id);
    m_entityMasks.erase(id);

    m_freeIds.push_back(id);
    std::erase_if(m_logicalIds, [id](const auto& pair) { return pair.second == id; });
}

bool Scene::IsAlive(EntityId id) const
{
    return m_entityMasks.find(id) != m_entityMasks.end();
}

Transform* Scene::AddTransform(EntityId id)
{
    if (!IsAlive(id))
    {
        return nullptr;
    }

    auto [it, inserted] = m_transforms.emplace(id, Transform{});
    Transform& transform = it->second;
    transform.MarkDirty();
    SetMaskBit(id, kTransformBit, true);
    return &transform;
}

Transform* Scene::GetTransform(EntityId id)
{
    auto it = m_transforms.find(id);
    if (it == m_transforms.end())
    {
        return nullptr;
    }
    return &it->second;
}

const Transform* Scene::GetTransform(EntityId id) const
{
    auto it = m_transforms.find(id);
    if (it == m_transforms.end())
    {
        return nullptr;
    }
    return &it->second;
}

void Scene::RemoveTransform(EntityId id)
{
    auto it = m_transforms.find(id);
    if (it != m_transforms.end())
    {
        m_transforms.erase(it);
        SetMaskBit(id, kTransformBit, false);
    }
}

MeshRenderer* Scene::AddMeshRenderer(EntityId id)
{
    if (!IsAlive(id))
    {
        return nullptr;
    }

    auto [it, inserted] = m_meshRenderers.emplace(id, MeshRenderer{});
    MeshRenderer& renderer = it->second;
    SetMaskBit(id, kMeshRendererBit, true);
    return &renderer;
}

MeshRenderer* Scene::GetMeshRenderer(EntityId id)
{
    auto it = m_meshRenderers.find(id);
    if (it == m_meshRenderers.end())
    {
        return nullptr;
    }
    return &it->second;
}

const MeshRenderer* Scene::GetMeshRenderer(EntityId id) const
{
    auto it = m_meshRenderers.find(id);
    if (it == m_meshRenderers.end())
    {
        return nullptr;
    }
    return &it->second;
}

void Scene::RemoveMeshRenderer(EntityId id)
{
    auto it = m_meshRenderers.find(id);
    if (it != m_meshRenderers.end())
    {
        m_meshRenderers.erase(it);
        SetMaskBit(id, kMeshRendererBit, false);
    }
}

void Scene::SetParent(EntityId child, EntityId parent)
{
    if (!IsAlive(child))
    {
        return;
    }

    if (parent != kInvalidEntity && !IsAlive(parent))
    {
        return;
    }

    EntityId currentParent = GetParent(child);
    if (currentParent == parent)
    {
        return;
    }

    if (currentParent != kInvalidEntity)
    {
        auto it = m_children.find(currentParent);
        if (it != m_children.end())
        {
            auto& siblings = it->second;
            siblings.erase(std::remove(siblings.begin(), siblings.end(), child), siblings.end());
        }
    }

    if (parent != kInvalidEntity)
    {
        m_children[parent].push_back(child);
        m_parents[child] = parent;
    }
    else
    {
        m_parents.erase(child);
    }

    MarkHierarchyDirty(child);
}

EntityId Scene::GetParent(EntityId child) const
{
    auto it = m_parents.find(child);
    if (it == m_parents.end())
    {
        return kInvalidEntity;
    }
    return it->second;
}

const std::vector<EntityId>& Scene::GetChildren(EntityId parent) const
{
    auto it = m_children.find(parent);
    if (it == m_children.end())
    {
        return kEmptyChildren;
    }
    return it->second;
}

size_t Scene::GetEntityCount() const
{
    return m_entityMasks.size();
}

size_t Scene::GetTransformCount() const
{
    return m_transforms.size();
}

size_t Scene::GetMeshRendererCount() const
{
    return m_meshRenderers.size();
}

size_t Scene::CountDirtyTransforms() const
{
    size_t dirty = 0;
    for (const auto& [id, transform] : m_transforms)
    {
        if (transform.dirty)
        {
            ++dirty;
        }
    }
    return dirty;
}

const std::unordered_map<EntityId, Transform>& Scene::GetTransforms() const
{
    return m_transforms;
}

std::unordered_map<EntityId, Transform>& Scene::GetTransforms()
{
    return m_transforms;
}

const std::unordered_map<EntityId, MeshRenderer>& Scene::GetMeshRenderers() const
{
    return m_meshRenderers;
}

std::unordered_map<EntityId, MeshRenderer>& Scene::GetMeshRenderers()
{
    return m_meshRenderers;
}

void Scene::SetLogicalLookup(std::unordered_map<std::string, EntityId> lookup)
{
    m_logicalIds = std::move(lookup);
}

EntityId Scene::FindEntityByLogicalId(const std::string& key) const
{
    auto it = m_logicalIds.find(key);
    if (it == m_logicalIds.end())
    {
        return kInvalidEntity;
    }
    return it->second;
}

void Scene::ForEachRootTransform(const std::function<void(EntityId)>& fn) const
{
    for (const auto& [entity, transform] : m_transforms)
    {
        const EntityId parent = GetParent(entity);
        if (parent == kInvalidEntity || !HasTransform(parent))
        {
            fn(entity);
        }
    }
}

void Scene::MarkHierarchyDirty(EntityId id)
{
    if (Transform* t = GetTransform(id))
    {
        t->MarkDirty();
    }

    auto it = m_children.find(id);
    if (it != m_children.end())
    {
        for (EntityId child : it->second)
        {
            MarkHierarchyDirty(child);
        }
    }
}

bool Scene::HasTransform(EntityId id) const
{
    return m_transforms.find(id) != m_transforms.end();
}

void Scene::SetMaskBit(EntityId id, size_t bit, bool value)
{
    auto it = m_entityMasks.find(id);
    if (it == m_entityMasks.end())
    {
        return;
    }
    it->second.set(bit, value);
}

