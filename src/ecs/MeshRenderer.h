#pragma once

#include "Entity.h"

struct Mesh;
struct Material;

struct MeshRenderer
{
    const Mesh*     mesh      = nullptr;
    const Material* material  = nullptr;
};

