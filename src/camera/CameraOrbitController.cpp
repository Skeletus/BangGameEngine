#include "CameraOrbitController.h"

#include "Camera.h"
#include "../ecs/Scene.h"
#include "../ecs/Transform.h"
#include "../input/InputSystem.h"
#include "../window/Window.h"
#include "../render/Renderer.h"

#include <nlohmann/json.hpp>
#include <bx/math.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <iostream>

using json = nlohmann::json;

namespace
{
    inline float DegToRad(float deg) { return bx::toRad(deg); }
}

CameraOrbitController::CameraOrbitController(Camera& camera,
                                             Scene& scene,
                                             InputSystem& input,
                                             Window& window,
                                             Renderer& renderer)
    : m_camera(camera)
    , m_scene(scene)
    , m_input(input)
    , m_window(window)
    , m_renderer(renderer)
{
    m_minPitchRad = bx::toRad(-85.0f);
    m_maxPitchRad = bx::toRad(-5.0f);
    m_minDistance = 1.5f;
    m_maxDistance = 12.0f;

    m_config.yawRad = bx::toRad(90.0f);
    m_config.pitchRad = bx::toRad(-20.0f);
    m_config.distance = 6.0f;
    m_config.sensLook = 1.0f;
    m_config.sensZoom = 1.0f;
    m_config.invertY = false;
    m_config.smoothing = true;
    m_config.smoothFactor = 8.0f;

    ResetToDefaults();
}

float CameraOrbitController::Clamp(float value, float minValue, float maxValue)
{
    return std::max(minValue, std::min(maxValue, value));
}

void CameraOrbitController::SetConfigPath(std::filesystem::path path)
{
    m_configPath = std::move(path);
    m_hasLastWriteTime = false;
    LoadConfig();
}

void CameraOrbitController::ReloadConfigIfNeeded()
{
    if (m_configPath.empty())
    {
        return;
    }

    std::error_code ec;
    auto currentTime = std::filesystem::last_write_time(m_configPath, ec);
    if (ec)
    {
        return;
    }

    if (!m_hasLastWriteTime || currentTime != m_lastWriteTime)
    {
        LoadConfig();
    }
}

void CameraOrbitController::LoadConfig()
{
    Config newConfig = m_config;

    if (!m_configPath.empty())
    {
        std::ifstream file(m_configPath);
        if (file)
        {
            try
            {
                json data;
                file >> data;

                const json* cameraJson = &data;
                if (data.contains("camera") && data["camera"].is_object())
                {
                    cameraJson = &data["camera"];
                }

                if (cameraJson->contains("targetId") && (*cameraJson)["targetId"].is_string())
                {
                    newConfig.targetId = (*cameraJson)["targetId"].get<std::string>();
                }
                if (cameraJson->contains("yawDeg") && (*cameraJson)["yawDeg"].is_number())
                {
                    newConfig.yawRad = DegToRad((*cameraJson)["yawDeg"].get<float>());
                }
                if (cameraJson->contains("pitchDeg") && (*cameraJson)["pitchDeg"].is_number())
                {
                    newConfig.pitchRad = DegToRad((*cameraJson)["pitchDeg"].get<float>());
                }
                if (cameraJson->contains("distance") && (*cameraJson)["distance"].is_number())
                {
                    newConfig.distance = (*cameraJson)["distance"].get<float>();
                }
                if (cameraJson->contains("sensLook") && (*cameraJson)["sensLook"].is_number())
                {
                    newConfig.sensLook = std::max(0.0f, (*cameraJson)["sensLook"].get<float>());
                }
                if (cameraJson->contains("sensZoom") && (*cameraJson)["sensZoom"].is_number())
                {
                    newConfig.sensZoom = std::max(0.0f, (*cameraJson)["sensZoom"].get<float>());
                }
                if (cameraJson->contains("invertY") && (*cameraJson)["invertY"].is_boolean())
                {
                    newConfig.invertY = (*cameraJson)["invertY"].get<bool>();
                }
                if (cameraJson->contains("smoothing") && (*cameraJson)["smoothing"].is_boolean())
                {
                    newConfig.smoothing = (*cameraJson)["smoothing"].get<bool>();
                }
                if (cameraJson->contains("smoothFactor") && (*cameraJson)["smoothFactor"].is_number())
                {
                    newConfig.smoothFactor = std::max(0.0f, (*cameraJson)["smoothFactor"].get<float>());
                }

            }
            catch (const std::exception& e)
            {
                std::cerr << "[CameraOrbit] Error parsing config: " << e.what() << '\n';
            }
        }
        else
        {
            std::cerr << "[CameraOrbit] Could not open config: " << m_configPath << '\n';
        }

        std::error_code ec;
        auto writeTime = std::filesystem::last_write_time(m_configPath, ec);
        if (!ec)
        {
            m_lastWriteTime = writeTime;
            m_hasLastWriteTime = true;
        }
    }

    newConfig.pitchRad = Clamp(newConfig.pitchRad, m_minPitchRad, m_maxPitchRad);
    newConfig.distance = Clamp(newConfig.distance, m_minDistance, m_maxDistance);

    m_config = newConfig;
    ResetToDefaults();
    ResolveTargetEntity();
    UpdateDebugHud();
}

void CameraOrbitController::ResetToDefaults()
{
    m_targetLogicalId = m_config.targetId;
    m_targetYaw = m_config.yawRad;
    m_targetPitch = Clamp(m_config.pitchRad, m_minPitchRad, m_maxPitchRad);
    m_targetDistance = Clamp(m_config.distance, m_minDistance, m_maxDistance);

    m_currentYaw = m_targetYaw;
    m_currentPitch = m_targetPitch;
    m_currentDistance = m_targetDistance;
}

void CameraOrbitController::OnSceneReloaded()
{
    ResolveTargetEntity();
}

void CameraOrbitController::ResolveTargetEntity()
{
    if (m_targetLogicalId.empty())
    {
        m_targetEntity = kInvalidEntity;
        return;
    }

    EntityId found = m_scene.FindEntityByLogicalId(m_targetLogicalId);
    if (found == kInvalidEntity)
    {
        if (m_targetEntity != kInvalidEntity)
        {
            std::printf("[CameraOrbit] Target '%s' not found.\n", m_targetLogicalId.c_str());
        }
        m_targetEntity = kInvalidEntity;
        return;
    }

    if (found != m_targetEntity)
    {
        std::printf("[CameraOrbit] Target resolved to entity %u for id '%s'.\n", found, m_targetLogicalId.c_str());
    }

    m_targetEntity = found;

    if (Transform* transform = m_scene.GetTransform(m_targetEntity))
    {
        m_lastTargetPos[0] = transform->world[12];
        m_lastTargetPos[1] = transform->world[13];
        m_lastTargetPos[2] = transform->world[14];
    }
}

void CameraOrbitController::Update(double dt)
{
    ReloadConfigIfNeeded();

    if (m_targetEntity != kInvalidEntity && !m_scene.IsAlive(m_targetEntity))
    {
        m_targetEntity = kInvalidEntity;
    }

    if (m_targetEntity == kInvalidEntity && !m_targetLogicalId.empty())
    {
        ResolveTargetEntity();
    }

    auto orbitLook = m_input.GetAction("OrbitLook");
    auto orbitReset = m_input.GetAction("OrbitReset");
    auto orbitCancel = m_input.GetAction("OrbitCancel");

    if (orbitLook.pressed)
    {
        if (!m_window.IsCursorLocked())
        {
            m_window.SetCursorLocked(true);
        }
        m_cursorLocked = true;
    }
    if ((!orbitLook.held && m_cursorLocked) || orbitCancel.pressed)
    {
        if (m_window.IsCursorLocked())
        {
            m_window.SetCursorLocked(false);
        }
        m_cursorLocked = false;
    }

    if (orbitReset.pressed)
    {
        ResetToDefaults();
        ResolveTargetEntity();
    }

    if (m_cursorLocked)
    {
        const float lookX = m_input.GetAxis("LookX");
        const float lookY = m_input.GetAxis("LookY");
        const float invert = m_config.invertY ? 1.0f : -1.0f;

        m_targetYaw += lookX * m_config.sensLook;
        m_targetPitch += lookY * m_config.sensLook * invert;
        m_targetPitch = Clamp(m_targetPitch, m_minPitchRad, m_maxPitchRad);
    }

    float zoomAxis = 0.0f;
    if (m_input.HasAxis("Zoom"))
    {
        zoomAxis = m_input.GetAxis("Zoom");
    }
    if (std::fabs(zoomAxis) > 1e-4f)
    {
        m_targetDistance = Clamp(m_targetDistance + zoomAxis * m_config.sensZoom, m_minDistance, m_maxDistance);
    }

    const float t = m_config.smoothing
        ? Clamp(1.0f - std::exp(-m_config.smoothFactor * static_cast<float>(dt)), 0.0f, 1.0f)
        : 1.0f;

    if (m_config.smoothing)
    {
        const float curCos = std::cos(m_currentYaw);
        const float curSin = std::sin(m_currentYaw);
        const float targetCos = std::cos(m_targetYaw);
        const float targetSin = std::sin(m_targetYaw);

        const float blendCos = curCos + (targetCos - curCos) * t;
        const float blendSin = curSin + (targetSin - curSin) * t;

        m_currentYaw = std::atan2(blendSin, blendCos);
        m_currentPitch = m_currentPitch + (m_targetPitch - m_currentPitch) * t;
        m_currentDistance = m_currentDistance + (m_targetDistance - m_currentDistance) * t;
    }
    else
    {
        m_currentYaw = m_targetYaw;
        m_currentPitch = m_targetPitch;
        m_currentDistance = m_targetDistance;
    }

    float targetPos[3] = { m_lastTargetPos[0], m_lastTargetPos[1], m_lastTargetPos[2] };
    if (m_targetEntity != kInvalidEntity)
    {
        if (Transform* transform = m_scene.GetTransform(m_targetEntity))
        {
            targetPos[0] = transform->world[12];
            targetPos[1] = transform->world[13];
            targetPos[2] = transform->world[14];
            m_lastTargetPos[0] = targetPos[0];
            m_lastTargetPos[1] = targetPos[1];
            m_lastTargetPos[2] = targetPos[2];
        }
    }

    const float cosYaw = std::cos(m_currentYaw);
    const float sinYaw = std::sin(m_currentYaw);
    const float cosPitch = std::cos(m_currentPitch);
    const float sinPitch = std::sin(m_currentPitch);

    float forward[3] = {
        cosYaw * cosPitch,
        sinPitch,
        sinYaw * cosPitch
    };

    float camPos[3] = {
        targetPos[0] - forward[0] * m_currentDistance,
        targetPos[1] - forward[1] * m_currentDistance,
        targetPos[2] - forward[2] * m_currentDistance
    };

    m_camera.SetPosition(camPos[0], camPos[1], camPos[2]);
    m_camera.SetYawPitch(m_currentYaw, m_currentPitch);

    UpdateDebugHud();
}

void CameraOrbitController::UpdateDebugHud()
{
    std::string targetInfo = m_targetLogicalId.empty() ? std::string("<none>") : m_targetLogicalId;
    if (m_targetEntity == kInvalidEntity && !m_targetLogicalId.empty())
    {
        targetInfo += " (missing)";
    }

    char buffer[128];
    std::snprintf(buffer, sizeof(buffer), "Orbit: yaw=%6.1f pitch=%6.1f dist=%4.2f target=%s",
                  bx::toDeg(m_currentYaw), bx::toDeg(m_currentPitch), m_currentDistance, targetInfo.c_str());
    m_debugLine = buffer;
    m_renderer.SetCameraOrbitDebugInfo(m_debugLine);
}
