#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct GLFWwindow;
class Window;

class InputSystem
{
public:
    struct ActionState
    {
        bool pressed  = false;
        bool held     = false;
        bool released = false;
    };

    InputSystem() = default;
    explicit InputSystem(Window* window);

    void SetWindow(Window* window);

    void LoadBindings(const std::string& path);
    void ReloadIfChanged();
    void Update(double dt);

    float GetAxis(std::string_view name) const;
    ActionState GetAction(std::string_view name) const;

    struct AxisBinding
    {
        enum class Type { Key, MouseDelta };
        enum class MouseAxis { DeltaX, DeltaY };

        Type type = Type::Key;
        int key = 0;
        MouseAxis mouseAxis = MouseAxis::DeltaX;
        float scale = 1.0f;
    };

private:
    

    struct AxisEntry
    {
        std::vector<AxisBinding> bindings;
        float value = 0.0f;
    };

    struct ActionBinding
    {
        int key = 0;
    };

    struct ActionEntry
    {
        std::vector<ActionBinding> bindings;
        bool previousHeld = false;
        ActionState state{};
    };

    void ResetMouseSmoothing();
    void UpdateActions();
    void UpdateAxes();

    Window* m_window = nullptr;
    GLFWwindow* m_glfwWindow = nullptr;

    std::string m_bindingPath;
    std::filesystem::file_time_type m_lastWriteTime{};
    bool m_hasLastWriteTime = false;

    struct MouseSettings
    {
        enum class SmoothType { None, Ema };
        float sensitivity = 0.1f;
        SmoothType smoothing = SmoothType::None;
        float alpha = 1.0f;
    } m_mouseSettings;

    float m_mouseSmoothedX = 0.0f;
    float m_mouseSmoothedY = 0.0f;
    bool  m_mouseInitialized = false;

    std::unordered_map<std::string, AxisEntry>   m_axes;
    std::unordered_map<std::string, ActionEntry> m_actions;
};

