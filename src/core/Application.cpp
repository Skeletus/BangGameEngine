#include "Application.h"
#include "Time.h"
#include "../window/Window.h"
#include "../render/Renderer.h"
#include "../camera/Camera.h"

#include <stdexcept>
#include <iostream>
#include <string>
#include <algorithm>
#include <cmath>
#include <GLFW/glfw3.h> // para códigos de tecla

Application::Application() {
    m_window = std::make_unique<Window>("SandboxCity - Initializing...", 1280, 720);
    m_renderer = std::make_unique<Renderer>();
    m_renderer->Init(m_window->GetNativeWindowHandle(), m_window->GetWidth(), m_window->GetHeight());

    m_camera = std::make_unique<Camera>();
    m_window->SetCursorLocked(true); // captura ratón para mirar

    // Proyección inicial
    const float aspect = (float)m_window->GetWidth() / (float)m_window->GetHeight();
    m_renderer->SetProjection(m_camera->GetFovYDeg(), aspect, m_camera->GetNear(), m_camera->GetFar());

    const char* backend = m_renderer->GetBackendName();
    std::string title = std::string("SandboxCity - Renderer: ") + (backend ? backend : "Unknown");
    m_window->SetTitle(title);
    std::cout << "[INFO] Renderer: " << (backend ? backend : "Unknown") << std::endl;
}

Application::~Application() {
    m_renderer.reset();
    m_window.reset();
}

void Application::Run() {
    Time::Init();

    while (m_running && !m_window->ShouldClose()) {
        Time::Tick();

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
            m_statusAccum = 0.0;
        }

        Render();
        m_window->PollEvents();
    }

    m_renderer->Shutdown();
}

void Application::Update(double dt) {
    // === Input ===
    const float moveSpeed = 5.0f;     // m/s
    const float mouseSens = 0.0025f;  // rad/pixel

    // Movimiento
    float dx = 0.0f, dy = 0.0f, dz = 0.0f;
    if (m_window->IsKeyDown(GLFW_KEY_W)) dz += 1.0f;
    if (m_window->IsKeyDown(GLFW_KEY_S)) dz -= 1.0f;
    if (m_window->IsKeyDown(GLFW_KEY_D)) dx += 1.0f;
    if (m_window->IsKeyDown(GLFW_KEY_A)) dx -= 1.0f;
    if (m_window->IsKeyDown(GLFW_KEY_SPACE))         dy += 1.0f;
    if (m_window->IsKeyDown(GLFW_KEY_LEFT_CONTROL))  dy -= 1.0f;

    // Normaliza plano XZ si hace falta (std::sqrtf en lugar de bx::fsqrt)
    const float lenSq = dx*dx + dz*dz;
    if (lenSq > 0.0f) {
        const float len = std::sqrt(std::max(lenSq, 1e-12f));
        dx /= len; dz /= len;
    }

    m_camera->Move(dx * moveSpeed * (float)dt,
                   dy * moveSpeed * (float)dt,
                   dz * moveSpeed * (float)dt);

    // Mirar con ratón (cuando el cursor está capturado)
    float mdx=0.0f, mdy=0.0f;
    m_window->GetMouseDelta(mdx, mdy);
    m_camera->AddYawPitch(mdx * mouseSens, -mdy * mouseSens);

    // Esc para liberar/recapturar cursor
    static bool escLatch = false;
    if (m_window->IsKeyDown(GLFW_KEY_ESCAPE)) {
        if (!escLatch) {
            escLatch = true;
            m_window->SetCursorLocked(false);
        }
    } else {
        escLatch = false;
    }

    // Aplica view a renderer
    float view[16];
    m_camera->GetView(view);
    m_renderer->SetView(view);

    // Debug info de cámara
    float camX, camY, camZ;
    m_camera->GetPosition(camX, camY, camZ);
    m_renderer->SetCameraDebugInfo(camX, camY, camZ);
}

void Application::Render() {
    // *** DEBUG: Imprime cada 60 frames ***
    static int frameCount = 0;
    if (++frameCount % 60 == 0) {
        float x, y, z;
        m_camera->GetPosition(x, y, z);
        printf("[DEBUG] Cam pos: (%.2f, %.2f, %.2f)\n", x, y, z);
    }
    
    m_renderer->BeginFrame();
    m_renderer->EndFrame();
}
