#pragma once

#include "../ecs/Entity.h"

#include <filesystem>
#include <string>

class Camera;
class Scene;
class InputSystem;
class Window;
class Renderer;

class CameraOrbitController
{
public:
    CameraOrbitController(Camera& camera,
                          Scene& scene,
                          InputSystem& input,
                          Window& window,
                          Renderer& renderer);

    void SetConfigPath(std::filesystem::path path);
    void ReloadConfigIfNeeded();
    void OnSceneReloaded();
    void Update(double dt);

private:
    struct Config
    {
        std::string targetId = "cj";
        float yawRad = 0.0f;
        float pitchRad = 0.0f;
        float distance = 6.0f;
        float sensLook = 1.0f;
        float sensZoom = 1.0f;
        bool invertY = false;
        bool smoothing = true;
        float smoothFactor = 8.0f;
    };

    void LoadConfig();
    void ResetToDefaults();
    void ResolveTargetEntity();
    void UpdateDebugHud();

    static float Clamp(float value, float minValue, float maxValue);

private:
    Camera&      m_camera;
    Scene&       m_scene;
    InputSystem& m_input;
    Window&      m_window;
    Renderer&    m_renderer;

    std::filesystem::path          m_configPath;
    std::filesystem::file_time_type m_lastWriteTime{};
    bool                           m_hasLastWriteTime = false;

    Config m_config{};

    float m_minPitchRad = 0.0f;
    float m_maxPitchRad = 0.0f;
    float m_minDistance = 1.5f;
    float m_maxDistance = 12.0f;

    float m_targetYaw = 0.0f;
    float m_targetPitch = 0.0f;
    float m_targetDistance = 6.0f;

    float m_currentYaw = 0.0f;
    float m_currentPitch = 0.0f;
    float m_currentDistance = 6.0f;

    std::string m_targetLogicalId;
    EntityId    m_targetEntity = kInvalidEntity;
    float       m_lastTargetPos[3] = {0.0f, 0.0f, 0.0f};

    bool        m_cursorLocked = false;
    std::string m_debugLine;
};
