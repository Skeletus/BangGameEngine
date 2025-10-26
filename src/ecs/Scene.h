#pragma once

#include "Entity.h"
#include "Transform.h"
#include "MeshRenderer.h"

#include <unordered_map>
#include <vector>
#include <bitset>
#include <functional>
#include <memory>

struct Mesh;
struct Material;

class Scene
{
public:
    Scene() = default;

    EntityId CreateEntity();
    void     DestroyEntity(EntityId id);
    bool     IsAlive(EntityId id) const;

    Transform*       AddTransform(EntityId id);
    Transform*       GetTransform(EntityId id);
    const Transform* GetTransform(EntityId id) const;
    void             RemoveTransform(EntityId id);

    MeshRenderer*       AddMeshRenderer(EntityId id);
    MeshRenderer*       GetMeshRenderer(EntityId id);
    const MeshRenderer* GetMeshRenderer(EntityId id) const;
    void                RemoveMeshRenderer(EntityId id);

    void     SetParent(EntityId child, EntityId parent);
    EntityId GetParent(EntityId child) const;
    const std::vector<EntityId>& GetChildren(EntityId parent) const;

    size_t GetEntityCount() const;
    size_t GetTransformCount() const;
    size_t GetMeshRendererCount() const;
    size_t CountDirtyTransforms() const;

    const std::unordered_map<EntityId, Transform>& GetTransforms() const;
    std::unordered_map<EntityId, Transform>&       GetTransforms();
    const std::unordered_map<EntityId, MeshRenderer>& GetMeshRenderers() const;
    std::unordered_map<EntityId, MeshRenderer>&       GetMeshRenderers();

    void ForEachRootTransform(const std::function<void(EntityId)>& fn) const;

    void MarkHierarchyDirty(EntityId id);

    bool HasTransform(EntityId id) const;

private:
    using ComponentMask = std::bitset<32>;

    void SetMaskBit(EntityId id, size_t bit, bool value);

private:
    std::unordered_map<EntityId, ComponentMask>    m_entityMasks;
    std::unordered_map<EntityId, Transform>        m_transforms;
    std::unordered_map<EntityId, MeshRenderer>     m_meshRenderers;
    std::unordered_map<EntityId, EntityId>         m_parents;
    std::unordered_map<EntityId, std::vector<EntityId>> m_children;
    std::vector<EntityId>                          m_freeIds;
    EntityId                                       m_nextId = kInvalidEntity;
};

