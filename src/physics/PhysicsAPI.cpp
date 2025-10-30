#include "PhysicsAPI.h"

#include "PhysicsSystem.h"
#include "../core/EventBus.h"

namespace
{
    PhysicsSystem* g_activeSystem = nullptr;
}

namespace Physics
{
    void SetActiveSystem(PhysicsSystem* system)
    {
        g_activeSystem = system;
    }

    bool Raycast(const float3& origin,
                 const float3& direction,
                 float maxDistance,
                 uint32_t layerMask,
                 PhysicsRaycastHit& outHit)
    {
        if (!g_activeSystem)
        {
            return false;
        }
        return g_activeSystem->Raycast(origin, direction, maxDistance, layerMask, outHit);
    }

    std::vector<PhysicsRaycastHit> RaycastAll(const float3& origin,
                                              const float3& direction,
                                              float maxDistance,
                                              uint32_t layerMask)
    {
        if (!g_activeSystem)
        {
            return {};
        }
        return g_activeSystem->RaycastAll(origin, direction, maxDistance, layerMask);
    }

    EventBus* GetEventBus()
    {
        if (!g_activeSystem)
        {
            return nullptr;
        }
        return &g_activeSystem->GetEventBus();
    }
}

