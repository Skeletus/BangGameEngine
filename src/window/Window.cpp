#include "Window.h"
#include <stdexcept>
#include <GLFW/glfw3.h>
#include <GLFW/glfw3native.h>  // requiere GLFW_EXPOSE_NATIVE_WIN32

Window::Window(const std::string& title, int width, int height)
    : m_width(width), m_height(height)
{
    if (!glfwInit())
        throw std::runtime_error("GLFW init failed");

    // *** sin contexto gráfico (bgfx decide el backend) ***
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

    m_window = glfwCreateWindow(m_width, m_height, title.c_str(), nullptr, nullptr);
    if (!m_window) {
        glfwTerminate();
        throw std::runtime_error("GLFW window creation failed");
    }

    glfwSetWindowUserPointer(m_window, this);
    glfwSetFramebufferSizeCallback(m_window, FramebufferSizeCallback);
    glfwSetScrollCallback(m_window, ScrollCallback);

    // Inicializa delta de ratón
    glfwGetCursorPos(m_window, &m_lastX, &m_lastY);
}

Window::~Window() {
    if (m_window) {
        glfwDestroyWindow(m_window);
        m_window = nullptr;
    }
    glfwTerminate();
}

void Window::PollEvents() {
    // Actualiza deltas de ratón antes de poll para tener valores “frescos”
    double x, y;
    glfwGetCursorPos(m_window, &x, &y);
    m_dx = x - m_lastX;
    m_dy = y - m_lastY;
    m_lastX = x;
    m_lastY = y;

    glfwPollEvents();
}

bool Window::ShouldClose() const { return glfwWindowShouldClose(m_window) == GLFW_TRUE; }

void* Window::GetNativeWindowHandle() const {
    return (void*)glfwGetWin32Window(m_window); // HWND
}

void Window::SetTitle(const std::string& title) {
    glfwSetWindowTitle(m_window, title.c_str());
}

bool Window::IsKeyDown(int glfwKey) const {
    return glfwGetKey(m_window, glfwKey) == GLFW_PRESS;
}

void Window::SetCursorLocked(bool locked) {
    m_cursorLocked = locked;
    glfwSetInputMode(m_window, GLFW_CURSOR, locked ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    // resetea deltas para evitar “saltos”
    glfwGetCursorPos(m_window, &m_lastX, &m_lastY);
    m_dx = m_dy = 0.0;
}

void Window::GetMouseDelta(float& dx, float& dy) const {
    if (!m_cursorLocked) { dx = dy = 0.0f; return; }
    dx = static_cast<float>(m_dx);
    dy = static_cast<float>(m_dy);
}

void Window::GetScrollDelta(float& sx, float& sy) const {
    sx = static_cast<float>(m_scrollX);
    sy = static_cast<float>(m_scrollY);
    m_scrollX = 0.0;
    m_scrollY = 0.0;
}

void Window::FramebufferSizeCallback(GLFWwindow* win, int w, int h) {
    auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(win));
    if (self) {
        self->m_width  = (w > 0) ? w : 1;
        self->m_height = (h > 0) ? h : 1;
    }
}

void Window::ScrollCallback(GLFWwindow* win, double xoffset, double yoffset) {
    auto* self = reinterpret_cast<Window*>(glfwGetWindowUserPointer(win));
    if (!self) {
        return;
    }
    self->m_scrollX += xoffset;
    self->m_scrollY += yoffset;
}
