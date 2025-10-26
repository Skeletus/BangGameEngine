#include "RenderSystem.h"

#include "Scene.h"
#include "Transform.h"
#include "MeshRenderer.h"
#include "../render/Renderer.h"
#include "../asset/Mesh.h"
#include "../render/Material.h"

void RenderSystem::Render(Scene& scene, Renderer& renderer)
{
    auto& renderers = scene.GetMeshRenderers();
    for (const auto& [entity, meshRenderer] : renderers)
    {
        if (!meshRenderer.mesh || !meshRenderer.material)
        {
            continue;
        }

        const Transform* transform = scene.GetTransform(entity);
        if (!transform)
        {
            continue;
        }

        renderer.SubmitMeshLit(*meshRenderer.mesh, *meshRenderer.material, transform->world);
    }
}

