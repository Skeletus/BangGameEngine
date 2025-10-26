#include "TransformSystem.h"

#include "Scene.h"
#include "Transform.h"

#include <bx/math.h>

namespace
{
    void UpdateNode(Scene& scene, EntityId entity, const float* parentWorld, bool parentDirty)
    {
        Transform* transform = scene.GetTransform(entity);
        if (!transform)
        {
            return;
        }

        bool localDirty = transform->dirty;
        if (localDirty)
        {
            transform->RecalculateLocalMatrix();
        }

        bool worldDirty = localDirty || parentDirty;
        if (worldDirty)
        {
            transform->UpdateWorldMatrix(parentWorld);
        }

        transform->dirty = false;

        const auto& children = scene.GetChildren(entity);
        for (EntityId child : children)
        {
            UpdateNode(scene, child, transform->world, worldDirty);
        }
    }
}

void TransformSystem::Update(Scene& scene)
{
    scene.ForEachRootTransform([&scene](EntityId entity)
    {
        UpdateNode(scene, entity, nullptr, false);
    });
}

