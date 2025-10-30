#pragma once

#include <cstdint>
#include <vector>

struct PhysicsDebugLine
{
    float    from[3];
    float    to[3];
    uint32_t abgr = 0xff000000u;
};

using PhysicsDebugLineBuffer = std::vector<PhysicsDebugLine>;