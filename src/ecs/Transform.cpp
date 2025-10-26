#include "Transform.h"

#include <bx/math.h>
#include <cstring>

Transform::Transform()
{
    bx::mtxIdentity(local);
    bx::mtxIdentity(world);
    dirty = true;
}

void Transform::MarkDirty()
{
    dirty = true;
}

void Transform::RecalculateLocalMatrix()
{
    bx::mtxSRT(local,
               scale.x, scale.y, scale.z,
               rotationEuler.x, rotationEuler.y, rotationEuler.z,
               position.x, position.y, position.z);
}

void Transform::UpdateWorldMatrix(const float* parentWorld)
{
    if (parentWorld)
    {
        bx::mtxMul(world, parentWorld, local);
    }
    else
    {
        std::memcpy(world, local, sizeof(world));
    }
}

