#pragma once

#include <string>

class Scene;
namespace resource { class ResourceManager; }

bool LoadSceneFromJson(const std::string& path,
                       Scene& scene,
                       resource::ResourceManager& resources,
                       std::string* err = nullptr);

