#pragma once

#include "Transform.h"

#include <cstdint>

enum class ColliderShape
{
    Box,
    Capsule,
};

struct Collider
{
    ColliderShape shape = ColliderShape::Box;
    // For boxes: half extents on each axis. For capsules: x = radius, y = half height.
    float3 size{0.5f, 0.5f, 0.5f};
    bool   dirty = true;
};

enum class RigidBodyType
{
    Static,
    Dynamic,
    Kinematic,
};

struct RigidBody
{
    RigidBodyType type  = RigidBodyType::Static;
    float         mass  = 0.0f;
    float         friction = 0.5f;
    float         restitution = 0.0f;
    uint32_t      layer = 1u;
    uint32_t      mask  = 0xffffffffu;
    bool          dirty = true;
};

struct TriggerVolume
{
    ColliderShape shape = ColliderShape::Box;
    float3        size{0.5f, 0.5f, 0.5f};
    uint32_t      layer = 0u;
    uint32_t      mask  = 0xffffffffu;
    bool          oneShot = false;
    bool          active  = true;
    bool          dirty   = true;
};

