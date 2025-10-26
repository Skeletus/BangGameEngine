#pragma once

#include "Entity.h"

#include <memory>

struct Mesh;
struct Material;

struct MeshRenderer
{
    std::shared_ptr<Mesh>     mesh;
    std::shared_ptr<Material> material;
};

