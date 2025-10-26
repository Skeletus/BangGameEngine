#pragma once

#include "Entity.h"

#include <memory>
#include <unordered_map>

struct Mesh;
struct Material;

struct MeshRenderer
{
    std::shared_ptr<Mesh>     mesh;
    std::shared_ptr<Material> material;
    std::unordered_map<uint32_t, std::shared_ptr<Material>> materialOverrides;
};

