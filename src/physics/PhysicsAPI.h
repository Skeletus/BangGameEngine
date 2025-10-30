#pragma once

#include "../ecs/Entity.h"
#include "../ecs/Transform.h"

#include <cstdint>
#include <vector>

class EventBus;
class PhysicsSystem;

struct PhysicsRaycastHit
{
    EntityId entity = kInvalidEntity;
    float3   point{0.0f, 0.0f, 0.0f};
    float3   normal{0.0f, 1.0f, 0.0f};
    float    distance = 0.0f;
};

namespace Physics
{
    bool Raycast(const float3& origin,
                 const float3& direction,
                 float maxDistance,
                 uint32_t layerMask,
                 PhysicsRaycastHit& outHit);

    std::vector<PhysicsRaycastHit> RaycastAll(const float3& origin,
                                              const float3& direction,
                                              float maxDistance,
                                              uint32_t layerMask);

    EventBus* GetEventBus();

    void SetActiveSystem(PhysicsSystem* system);
}

