#include "Application.h"
#include "Time.h"
#include "../window/Window.h"
#include "../render/Renderer.h"
#include "../resource/ResourceManager.h"
#include "../camera/Camera.h"
#include "../camera/CameraOrbitController.h"
#include "../ecs/TransformSystem.h"
#include "../ecs/Scene.h"
#include "../scene/SceneLoader.h"
#include "../physics/PhysicsAPI.h"

#include <cstdio>

#include <stdexcept>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <GLFW/glfw3.h> // para códigos de tecla
#include <bx/math.h>

Application::Application() {
    m_window = std::make_unique<Window>("SandboxCity - Initializing...", 1280, 720);
    m_renderer = std::make_unique<Renderer>();
    m_renderer->Init(m_window->GetNativeWindowHandle(), m_window->GetWidth(), m_window->GetHeight());

    m_input.SetWindow(m_window.get());
    m_input.LoadBindings("../../../assets/input/bindings.json");

    m_physics.SetConfigPath("../../../assets/config/physics.json");
    m_physics.Initialize();

    if (EventBus* bus = Physics::GetEventBus())
    {
        bus->Subscribe<PhysicsSystem::TriggerEvent>([this](const PhysicsSystem::TriggerEvent& evt)
        {
            OnTriggerEvent(evt);
        });
    }

    m_resourceManager = std::make_unique<resource::ResourceManager>();
    m_resourceManager->Initialize();
    m_renderer->SetResourceManager(m_resourceManager.get());

    m_scenePath = "assets/scenes/demo.json";
    ReloadScene("inicial");
    
    m_camera = std::make_unique<Camera>();
    m_window->SetCursorLocked(false);

    m_cameraOrbit = std::make_unique<CameraOrbitController>(*m_camera, m_scene, m_input, *m_window, *m_renderer);
    m_cameraOrbit->SetConfigPath("../../../assets/config/camera.json");
    m_cameraOrbit->OnSceneReloaded();

    // Proyección inicial
    const float aspect = (float)m_window->GetWidth() / (float)m_window->GetHeight();
    m_renderer->SetProjection(m_camera->GetFovYDeg(), aspect, m_camera->GetNear(), m_camera->GetFar());

    const char* backend = m_renderer->GetBackendName();
    std::string title = std::string("SandboxCity - Renderer: ") + (backend ? backend : "Unknown");
    m_window->SetTitle(title);
    std::cout << "[INFO] Renderer: " << (backend ? backend : "Unknown") << std::endl;
}

Application::~Application() {
    if (m_resourceManager) m_resourceManager->Shutdown();
    m_resourceManager.reset();
    if (m_renderer) m_renderer->Shutdown(); // <- extra seguro
    m_renderer.reset();
    m_window.reset();
}

void Application::Run() {
    Time::Init();

    while (m_running && !m_window->ShouldClose()) {
        Time::Tick();

        m_input.ReloadIfChanged();
        m_input.Update(Time::DeltaTime());

        if (m_physics.ReloadConfigIfNeeded(m_scene))
        {
            m_fixedDt = m_physics.GetFixedStep();
        }

        // Resize & proyección
        int w = m_window->GetWidth();
        int h = m_window->GetHeight();
        m_renderer->OnResize((uint32_t)w, (uint32_t)h);
        const float aspect = (h > 0) ? (float)w / (float)h : 16.0f/9.0f;
        m_renderer->SetProjection(m_camera->GetFovYDeg(), aspect, m_camera->GetNear(), m_camera->GetFar());

        // Fixed update 60 Hz
        m_accum += Time::DeltaTime();
        while (m_accum >= m_fixedDt) {
            Update(m_fixedDt);
            m_accum -= m_fixedDt;
        }

        // HUD de consola/título cada 0.5s
        m_statusAccum += Time::DeltaTime();
        if (m_statusAccum >= 0.5) {
            const char* backend = m_renderer->GetBackendName();
            const double fps = Time::FPS();
            std::string title = "SandboxCity - Renderer: ";
            title += (backend ? backend : "Unknown");
            title += "  |  FPS: ";
            title += std::to_string(static_cast<int>(fps + 0.5));
            m_window->SetTitle(title);
            std::cout << "[INFO] Renderer: " << (backend ? backend : "Unknown")
                      << " | FPS: " << fps << std::endl;
            std::cout << "[ECS] Entities: " << m_lastEntityCount
                      << " | Transforms: " << m_lastTransformCount
                      << " | MeshRenderers: " << m_lastMeshRendererCount
                      << " | Dirty (pre/post): " << m_lastDirtyBefore
                      << " -> " << m_lastDirtyAfter
                      << (m_lastDirtyAfter == 0 ? " [OK]" : " [WARN]")
                      << std::endl;
            m_statusAccum = 0.0;
        }

        if (m_renderer)
        {
            const float moveForward = m_input.GetAxis("MoveForward");
            const float moveRight   = m_input.GetAxis("MoveRight");
            const float lookX       = m_input.GetAxis("LookX");
            const float lookY       = m_input.GetAxis("LookY");
            const auto  jump        = m_input.GetAction("Jump");
            const auto  sprint      = m_input.GetAction("Sprint");

            char buffer[160];
            std::snprintf(buffer, sizeof(buffer),
                "MF/MR=%5.2f/%5.2f  LX/LY=%5.2f/%5.2f  Jump[P/H/R]=%d/%d/%d  Sprint[H]=%d",
                moveForward, moveRight, lookX, lookY,
                jump.pressed ? 1 : 0,
                jump.held ? 1 : 0,
                jump.released ? 1 : 0,
                sprint.held ? 1 : 0);

            m_renderer->SetInputDebugInfo(buffer);
        }

        Render();
        m_window->PollEvents();
    }

    // NO llamar a m_renderer->Shutdown() aquí; el destructor se encarga.
}

void Application::Update(double dt) {
    // === Input ===
    if (m_cameraOrbit)
    {
        m_cameraOrbit->Update(dt);
    }

    // === Toggles existentes ===
    static bool f1Latch=false, vLatch=false;

    if (m_window->IsKeyDown(GLFW_KEY_F1)) {
        if (!f1Latch) { f1Latch = true; m_renderer->ToggleWireframe(); }
    } else f1Latch = false;

    static bool f3Latch = false;
    if (m_window->IsKeyDown(GLFW_KEY_F3))
    {
        if (!f3Latch)
        {
            f3Latch = true;
            m_physics.ToggleDebugOverlay();
        }
    }
    else
    {
        f3Latch = false;
    }

    if (m_window->IsKeyDown(GLFW_KEY_V)) {
        if (!vLatch) { vLatch = true; m_renderer->ToggleVsync(); }
    } else vLatch = false;

    static bool f9Latch = false;
    if (m_window->IsKeyDown(GLFW_KEY_F9))
    {
        if (!f9Latch)
        {
            f9Latch = true;
            if (m_resourceManager)
            {
                m_resourceManager->PrintStats();
            }
            m_physics.LogStats();
        }
    }
    else
    {
        f9Latch = false;
    }

    static bool f5Latch = false;
    if (m_window->IsKeyDown(GLFW_KEY_F5))
    {
        if (!f5Latch)
        {
            f5Latch = true;
            ReloadScene("recargada");
        }
    }
    else
    {
        f5Latch = false;
    }

    // === Controles de iluminación en runtime ===
    const float rotSpeed = bx::toRad(90.0f);      // deg/s -> rad/s
    const float ambSpeed = 0.8f;                  // por segundo
    const float specISpd = 1.2f;                  // por segundo
    const float shinySpd = 128.0f;                // unidades por segundo

    // Girar luz con flechas
    if (m_window->IsKeyDown(GLFW_KEY_LEFT))  m_renderer->AddLightYawPitch(-rotSpeed*(float)dt, 0.0f);
    if (m_window->IsKeyDown(GLFW_KEY_RIGHT)) m_renderer->AddLightYawPitch( rotSpeed*(float)dt, 0.0f);
    if (m_window->IsKeyDown(GLFW_KEY_UP))    m_renderer->AddLightYawPitch(0.0f, -rotSpeed*(float)dt*0.5f);
    if (m_window->IsKeyDown(GLFW_KEY_DOWN))  m_renderer->AddLightYawPitch(0.0f,  rotSpeed*(float)dt*0.5f);

    // Ambient Z/X
    if (m_window->IsKeyDown(GLFW_KEY_Z)) m_renderer->AdjustAmbient(-ambSpeed*(float)dt);
    if (m_window->IsKeyDown(GLFW_KEY_X)) m_renderer->AdjustAmbient( ambSpeed*(float)dt);

    // Spec intensity C/V
    if (m_window->IsKeyDown(GLFW_KEY_C)) m_renderer->AdjustSpecIntensity(-specISpd*(float)dt);
    if (m_window->IsKeyDown(GLFW_KEY_V)) m_renderer->AdjustSpecIntensity( specISpd*(float)dt);

    // Shininess B/N
    if (m_window->IsKeyDown(GLFW_KEY_B)) m_renderer->AdjustShininess(-shinySpd*(float)dt);
    if (m_window->IsKeyDown(GLFW_KEY_N)) m_renderer->AdjustShininess( shinySpd*(float)dt);

    // Reset R (con latch)
    static bool rLatch=false;
    if (m_window->IsKeyDown(GLFW_KEY_R)) {
        if (!rLatch) { rLatch = true; m_renderer->ResetLightingDefaults(); }
    } else rLatch=false;

    // Aplica view a renderer + debug de cámara
    float view[16];
    m_camera->GetView(view);
    m_renderer->SetView(view);

    float camX, camY, camZ;
    m_camera->GetPosition(camX, camY, camZ);
    m_renderer->SetCameraDebugInfo(camX, camY, camZ);

    m_physics.Update(m_scene, *m_camera, m_input, dt);

    std::string physicsLine;
    PhysicsRaycastHit rayHit{};
    float3 origin{camX, camY, camZ};
    float3 dir{0.0f, -1.0f, 0.0f};
    constexpr uint32_t kWorldLayerMask = 1u;
    if (Physics::Raycast(origin, dir, 200.0f, kWorldLayerMask, rayHit))
    {
        const std::string label = GetEntityLabel(rayHit.entity);
        char buffer[128];
        std::snprintf(buffer, sizeof(buffer),
                      "Raycast: %s @ (%.2f, %.2f, %.2f) d=%.2f",
                      label.c_str(),
                      rayHit.point.x, rayHit.point.y, rayHit.point.z,
                      rayHit.distance);
        physicsLine = buffer;
    }
    else
    {
        physicsLine = "Raycast: sin impacto";
    }
    if (m_renderer)
    {
        m_renderer->SetPhysicsDebugInfo(physicsLine);
    }

    m_lastDirtyBefore = m_scene.CountDirtyTransforms();
    TransformSystem::Update(m_scene);
    m_lastDirtyAfter = m_scene.CountDirtyTransforms();

#ifdef SANDBOXCITY_ECS_DEMO
    if (m_lastDirtyAfter != 0)
    {
        std::printf("[ECS] ALERTA: dirty tras Update = %zu\n", m_lastDirtyAfter);
    }
#endif

    m_lastEntityCount       = m_scene.GetEntityCount();
    m_lastTransformCount    = m_scene.GetTransformCount();
    m_lastMeshRendererCount = m_scene.GetMeshRendererCount();
}

void Application::ReloadScene(const char* reason)
{
    if (!m_resourceManager)
    {
        std::printf("[App] ResourceManager no disponible para recargar escena.\n");
        return;
    }

    const std::string sceneFile = m_scenePath.empty() ? std::string("assets/scenes/demo.json") : m_scenePath;
    std::string error;
    if (!LoadSceneFromJson(sceneFile, m_scene, *m_resourceManager, &error))
    {
        std::printf("[App] Error al cargar escena '%s': %s\n", sceneFile.c_str(), error.c_str());
        return;
    }

    TransformSystem::Update(m_scene);
    m_lastEntityCount       = m_scene.GetEntityCount();
    m_lastTransformCount    = m_scene.GetTransformCount();
    m_lastMeshRendererCount = m_scene.GetMeshRendererCount();
    PrintSceneSummary(reason);

    m_cjEntity = m_scene.FindEntityByLogicalId("cj");
    m_checkpointEntity = m_scene.FindEntityByLogicalId("checkpoint");

    m_physics.OnSceneReloaded(m_scene);
    m_physics.ReloadConfigIfNeeded(m_scene);
    m_fixedDt = m_physics.GetFixedStep();

    if (m_cameraOrbit)
    {
        m_cameraOrbit->OnSceneReloaded();
    }
}

void Application::PrintSceneSummary(const char* reason)
{
    const char* label = reason ? reason : "actualizada";
    std::printf("[App] Escena %s: Entities=%zu | Transforms=%zu | MeshRenderers=%zu\n",
                label,
                m_scene.GetEntityCount(),
                m_scene.GetTransformCount(),
                m_scene.GetMeshRendererCount());
    if (m_resourceManager)
    {
        m_resourceManager->PrintStats();
    }
}

void Application::Render() {
    static int frameCount = 0;
    if (++frameCount % 60 == 0) {
        float x, y, z;
        m_camera->GetPosition(x, y, z);
        printf("[DEBUG] Cam pos: (%.2f, %.2f, %.2f)\n", x, y, z);
    }
    if (!m_renderer) return;

    m_renderer->BeginFrame(&m_scene);

    const PhysicsDebugLineBuffer& debugLines = m_physics.GetDebugLines();
    m_renderer->DrawDebugLines(debugLines);

    m_renderer->EndFrame();
}

void Application::OnTriggerEvent(const PhysicsSystem::TriggerEvent& evt)
{
    if (evt.trigger == m_checkpointEntity && evt.other == m_cjEntity)
    {
        switch (evt.type)
        {
        case PhysicsSystem::TriggerEvent::Type::Enter:
            std::printf("[Trigger] CJ entró al Checkpoint\n");
            break;
        case PhysicsSystem::TriggerEvent::Type::Exit:
            std::printf("[Trigger] CJ salió del Checkpoint\n");
            break;
        default:
            break;
        }
    }
}

std::string Application::GetEntityLabel(EntityId id) const
{
    if (id == kInvalidEntity)
    {
        return "N/A";
    }

    for (const auto& [label, entity] : m_scene.GetLogicalLookup())
    {
        if (entity == id)
        {
            return label;
        }
    }

    return std::string("#") + std::to_string(id);
}
