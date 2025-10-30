#pragma once
#include <memory>
#include <string>
#include <cstddef>

#include "../ecs/Scene.h"
#include "../input/InputSystem.h"
#include "../physics/PhysicsSystem.h"

struct Material;

class Window;
class Renderer;
class Camera;
class CameraOrbitController;
namespace resource { class ResourceManager; }

class Application {
public:
    Application();
    ~Application();
    void Run();

private:
    void Update(double dt);
    void Render();
    void ReloadScene(const char* reason);
    void PrintSceneSummary(const char* reason);
    void OnTriggerEvent(const PhysicsSystem::TriggerEvent& evt);
    std::string GetEntityLabel(EntityId id) const;

private:
    std::unique_ptr<Window>                      m_window;
    std::unique_ptr<Renderer>                    m_renderer;
    std::unique_ptr<Camera>                      m_camera;
    std::unique_ptr<CameraOrbitController>       m_cameraOrbit;
    std::unique_ptr<resource::ResourceManager>   m_resourceManager;

    InputSystem   m_input;
    PhysicsSystem m_physics;

    Scene     m_scene;
    std::string m_scenePath;

    EntityId m_cjEntity = kInvalidEntity;
    EntityId m_checkpointEntity = kInvalidEntity;

    size_t m_lastEntityCount        = 0;
    size_t m_lastTransformCount     = 0;
    size_t m_lastMeshRendererCount  = 0;
    size_t m_lastDirtyBefore        = 0;
    size_t m_lastDirtyAfter         = 0;

    bool   m_running = true;
    double m_accum   = 0.0;
    double m_fixedDt = 1.0 / 60.0;

    double m_statusAccum = 0.0;
};
