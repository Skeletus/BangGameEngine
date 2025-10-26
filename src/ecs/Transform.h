#pragma once

#include "Entity.h"

struct float3
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;
};

struct Transform
{
    float3 position{0.0f, 0.0f, 0.0f};
    float3 rotationEuler{0.0f, 0.0f, 0.0f};
    float3 scale{1.0f, 1.0f, 1.0f};
    float  local[16]{};
    float  world[16]{};
    bool   dirty = true;

    Transform();

    void MarkDirty();
    void RecalculateLocalMatrix();
    void UpdateWorldMatrix(const float* parentWorld);
};

