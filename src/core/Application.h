#pragma once
#include <memory>
#include <string>

class Window;
class Renderer;
class Camera;

class Application {
public:
    Application();
    ~Application();
    void Run();

private:
    void Update(double dt);
    void Render();

private:
    std::unique_ptr<Window>   m_window;
    std::unique_ptr<Renderer> m_renderer;
    std::unique_ptr<Camera>   m_camera;

    bool   m_running = true;
    double m_accum   = 0.0;
    double m_fixedDt = 1.0 / 60.0;

    double m_statusAccum = 0.0;
};
