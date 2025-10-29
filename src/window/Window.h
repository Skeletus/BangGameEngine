#pragma once
#include <string>
#include <cstdint>

struct GLFWwindow;

class Window {
public:
    Window(const std::string& title, int width, int height);
    ~Window();

    void PollEvents();
    bool ShouldClose() const;

    int  GetWidth()  const { return m_width; }
    int  GetHeight() const { return m_height; }

    void* GetNativeWindowHandle() const; // HWND en Windows
    GLFWwindow* GetGlfwHandle() const { return m_window; }
    void  SetTitle(const std::string& title);

    // === INPUT BÁSICO ===
    bool  IsKeyDown(int glfwKey) const;                // GLFW_KEY_*
    void  SetCursorLocked(bool locked);                // captura/oculta cursor
    void  GetMouseDelta(float& dx, float& dy) const;   // desde el último frame
    void  GetScrollDelta(float& sx, float& sy) const;  // acumulado desde último frame
    bool  IsCursorLocked() const { return m_cursorLocked; }

private:
    static void FramebufferSizeCallback(GLFWwindow* win, int w, int h);
    static void ScrollCallback(GLFWwindow* win, double xoffset, double yoffset);

private:
    GLFWwindow* m_window = nullptr;
    int m_width = 0;
    int m_height = 0;

    // estado de ratón
    mutable double m_lastX = 0.0, m_lastY = 0.0;
    mutable double m_dx = 0.0,  m_dy = 0.0;
    mutable double m_scrollX = 0.0, m_scrollY = 0.0;
    bool m_cursorLocked = false;
};
