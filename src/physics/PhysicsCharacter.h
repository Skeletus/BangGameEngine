#pragma once

#include "../ecs/Entity.h"

class btPairCachingGhostObject;
class btKinematicCharacterController;

struct PhysicsCharacter
{
    EntityId entity = kInvalidEntity;
    btPairCachingGhostObject* ghost = nullptr;
    btKinematicCharacterController* controller = nullptr;
    float walkSpeed = 3.5f;
    float jumpImpulse = 5.0f;
    bool  dirty = true;
};

