#include "InputSystem.h"

#include "../window/Window.h"

#include <GLFW/glfw3.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <fstream>
#include <iostream>
#include <optional>
#include <unordered_map>

using json = nlohmann::json;

namespace
{
    std::string ToUpper(std::string_view text)
    {
        std::string result(text);
        std::transform(result.begin(), result.end(), result.begin(), [](unsigned char c) { return static_cast<char>(std::toupper(c)); });
        return result;
    }

    std::optional<int> KeyFromString(std::string_view name)
    {
        std::string upper = ToUpper(name);

        if (upper.size() == 1)
        {
            char c = upper[0];
            if (c >= 'A' && c <= 'Z')
            {
                return GLFW_KEY_A + (c - 'A');
            }
            if (c >= '0' && c <= '9')
            {
                return GLFW_KEY_0 + (c - '0');
            }
        }

        static const std::unordered_map<std::string, int> kNamedKeys = {
            {"SPACE", GLFW_KEY_SPACE},
            {"LEFT_SHIFT", GLFW_KEY_LEFT_SHIFT},
            {"RIGHT_SHIFT", GLFW_KEY_RIGHT_SHIFT},
            {"LEFT_CONTROL", GLFW_KEY_LEFT_CONTROL},
            {"RIGHT_CONTROL", GLFW_KEY_RIGHT_CONTROL},
            {"LEFT_ALT", GLFW_KEY_LEFT_ALT},
            {"RIGHT_ALT", GLFW_KEY_RIGHT_ALT},
            {"TAB", GLFW_KEY_TAB},
            {"ENTER", GLFW_KEY_ENTER},
            {"RETURN", GLFW_KEY_ENTER},
            {"ESCAPE", GLFW_KEY_ESCAPE},
            {"UP", GLFW_KEY_UP},
            {"DOWN", GLFW_KEY_DOWN},
            {"LEFT", GLFW_KEY_LEFT},
            {"RIGHT", GLFW_KEY_RIGHT},
            {"F1", GLFW_KEY_F1},
            {"F2", GLFW_KEY_F2},
            {"F3", GLFW_KEY_F3},
            {"F4", GLFW_KEY_F4},
            {"F5", GLFW_KEY_F5},
            {"F6", GLFW_KEY_F6},
            {"F7", GLFW_KEY_F7},
            {"F8", GLFW_KEY_F8},
            {"F9", GLFW_KEY_F9},
            {"F10", GLFW_KEY_F10},
            {"F11", GLFW_KEY_F11},
            {"F12", GLFW_KEY_F12},
        };

        auto it = kNamedKeys.find(upper);
        if (it != kNamedKeys.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    InputSystem::AxisBinding::MouseAxis MouseAxisFromString(std::string_view name)
    {
        std::string upper = ToUpper(name);
        if (upper == "DELTAX")
        {
            return InputSystem::AxisBinding::MouseAxis::DeltaX;
        }
        return InputSystem::AxisBinding::MouseAxis::DeltaY;
    }
}

InputSystem::InputSystem(Window* window)
{
    SetWindow(window);
}

void InputSystem::SetWindow(Window* window)
{
    m_window = window;
    m_glfwWindow = nullptr;
    if (m_window)
    {
        m_glfwWindow = m_window->GetGlfwHandle();
    }
}

void InputSystem::LoadBindings(const std::string& path)
{
    std::printf("[DEBUG_LOAD] LoadBindings() called with path: %s\n", path.c_str());
    
    m_bindingPath = path;

    m_axes.clear();
    m_actions.clear();
    ResetMouseSmoothing();

    std::ifstream file(path);
    if (!file.is_open())
    {
        std::printf("[ERROR] Failed to open bindings file: %s\n", path.c_str());
        std::cerr << "[Input] Failed to open bindings file: " << path << '\n';
        return;
    }
    std::printf("[DEBUG_LOAD] File opened successfully\n");

    json data;
    try
    {
        file >> data;
        std::printf("[DEBUG_LOAD] JSON parsed successfully\n");
    }
    catch (const std::exception& e)
    {
        std::printf("[ERROR] Failed to parse JSON: %s\n", e.what());
        std::cerr << "[Input] Failed to parse JSON: " << e.what() << '\n';
        return;
    }

    if (auto mouseIt = data.find("mouse"); mouseIt != data.end() && mouseIt->is_object())
    {
        m_mouseSettings.sensitivity = mouseIt->value("sensitivity", m_mouseSettings.sensitivity);
        std::string smooth = ToUpper(mouseIt->value("smoothtype", std::string("none")));
        std::printf("[DEBUG_LOAD] Mouse settings: sensitivity=%.2f, smoothtype=%s\n", 
                    m_mouseSettings.sensitivity, smooth.c_str());
        if (smooth == "EMA")
        {
            m_mouseSettings.smoothing = MouseSettings::SmoothType::Ema;
            m_mouseSettings.alpha = std::clamp(mouseIt->value("alpha", 1.0f), 0.0f, 1.0f);
        }
        else
        {
            m_mouseSettings.smoothing = MouseSettings::SmoothType::None;
            m_mouseSettings.alpha = 1.0f;
        }
    }
    else
    {
        std::printf("[DEBUG_LOAD] No 'mouse' section found in JSON\n");
        m_mouseSettings.sensitivity = 0.1f;
        m_mouseSettings.smoothing = MouseSettings::SmoothType::None;
        m_mouseSettings.alpha = 1.0f;
    }

    if (auto axesIt = data.find("axes"); axesIt != data.end() && axesIt->is_object())
    {
        std::printf("[DEBUG_LOAD] Found 'axes' section\n");
        for (auto& [axisName, bindings] : axesIt->items())
        {
            std::printf("[DEBUG_LOAD] Loading axis '%s'\n", axisName.c_str());
            AxisEntry entry;
            if (bindings.is_array())
            {
                std::printf("[DEBUG_LOAD]   Axis '%s' has %zu bindings\n", axisName.c_str(), bindings.size());
                for (const auto& binding : bindings)
                {
                    AxisBinding axisBinding;
                    axisBinding.scale = binding.value("scale", 1.0f);
                    std::printf("[DEBUG_LOAD]     Binding scale: %.2f\n", axisBinding.scale);
                    
                    if (auto keyIt = binding.find("key"); keyIt != binding.end() && keyIt->is_string())
                    {
                        auto keyCode = KeyFromString(*keyIt);
                        if (keyCode)
                        {
                            axisBinding.type = AxisBinding::Type::Key;
                            axisBinding.key = *keyCode;
                            entry.bindings.push_back(axisBinding);
                            std::printf("[DEBUG_LOAD]     Added KEY binding: %s (code=%d)\n", keyIt->get<std::string>().c_str(), *keyCode);
                        }
                        else
                        {
                            std::printf("[ERROR] Unknown key in axis '%s': %s\n", axisName.c_str(), keyIt->get<std::string>().c_str());
                            std::cerr << "[Input] Unknown key in axis '" << axisName << "': " << *keyIt << '\n';
                        }
                    }
                    else if (auto mouseIt = binding.find("mouse"); mouseIt != binding.end() && mouseIt->is_string())
                    {
                        axisBinding.type = AxisBinding::Type::MouseDelta;
                        axisBinding.mouseAxis = MouseAxisFromString(*mouseIt);
                        entry.bindings.push_back(axisBinding);
                        std::printf("[DEBUG_LOAD]     Added MOUSE binding: %s\n", mouseIt->get<std::string>().c_str());
                    }
                }
            }
            m_axes.emplace(axisName, std::move(entry));
            std::printf("[DEBUG_LOAD] Axis '%s' loaded with %zu bindings\n", axisName.c_str(), entry.bindings.size());
        }
        std::printf("[DEBUG_LOAD] Total axes loaded: %zu\n", m_axes.size());
    }
    else
    {
        std::printf("[ERROR] No 'axes' section found in JSON\n");
    }

    if (auto actionsIt = data.find("actions"); actionsIt != data.end() && actionsIt->is_object())
    {
        std::printf("[DEBUG_LOAD] Found 'actions' section\n");
        for (auto& [actionName, bindings] : actionsIt->items())
        {
            std::printf("[DEBUG_LOAD] Loading action '%s'\n", actionName.c_str());
            ActionEntry entry;
            if (bindings.is_array())
            {
                for (const auto& binding : bindings)
                {
                    if (auto keyIt = binding.find("key"); keyIt != binding.end() && keyIt->is_string())
                    {
                        auto keyCode = KeyFromString(*keyIt);
                        if (keyCode)
                        {
                            ActionBinding actionBinding;
                            actionBinding.key = *keyCode;
                            entry.bindings.push_back(actionBinding);
                            std::printf("[DEBUG_LOAD]   Added action binding: %s\n", keyIt->get<std::string>().c_str());
                        }
                        else
                        {
                            std::printf("[ERROR] Unknown key in action '%s': %s\n", actionName.c_str(), keyIt->get<std::string>().c_str());
                            std::cerr << "[Input] Unknown key in action '" << actionName << "': " << *keyIt << '\n';
                        }
                    }
                }
            }
            m_actions.emplace(actionName, std::move(entry));
        }
        std::printf("[DEBUG_LOAD] Total actions loaded: %zu\n", m_actions.size());
    }
    else
    {
        std::printf("[ERROR] No 'actions' section found in JSON\n");
    }

    std::error_code ec;
    auto lastWrite = std::filesystem::last_write_time(path, ec);
    if (!ec)
    {
        m_lastWriteTime = lastWrite;
        m_hasLastWriteTime = true;
        std::printf("[DEBUG_LOAD] File modification time tracked\n");
    }
    else
    {
        m_hasLastWriteTime = false;
        std::printf("[DEBUG_LOAD] Could not track file modification time\n");
    }
    
    std::printf("[DEBUG_LOAD] LoadBindings() complete. Axes: %zu, Actions: %zu\n", 
                m_axes.size(), m_actions.size());
}

void InputSystem::ReloadIfChanged()
{
    if (m_bindingPath.empty())
    {
        return;
    }

    std::error_code ec;
    auto currentWrite = std::filesystem::last_write_time(m_bindingPath, ec);
    if (ec)
    {
        return;
    }

    if (!m_hasLastWriteTime || currentWrite != m_lastWriteTime)
    {
        LoadBindings(m_bindingPath);
    }
}

void InputSystem::Update(double)
{
    UpdateActions();
    UpdateAxes();
}

float InputSystem::GetAxis(std::string_view name) const
{
    std::string key(name);
    auto it = m_axes.find(key);
    if (it == m_axes.end())
    {
        return 0.0f;
    }
    return it->second.value;
}

InputSystem::ActionState InputSystem::GetAction(std::string_view name) const
{
    std::string key(name);
    auto it = m_actions.find(key);
    if (it == m_actions.end())
    {
        return ActionState{};
    }
    return it->second.state;
}

void InputSystem::ResetMouseSmoothing()
{
    m_mouseSmoothedX = 0.0f;
    m_mouseSmoothedY = 0.0f;
    m_mouseInitialized = false;
}

void InputSystem::UpdateActions()
{
    if (!m_glfwWindow)
    {
        for (auto& [_, entry] : m_actions)
        {
            entry.state.pressed = false;
            entry.state.released = entry.previousHeld;
            entry.state.held = false;
            entry.previousHeld = false;
        }
        return;
    }

    for (auto& [_, entry] : m_actions)
    {
        bool held = false;
        for (const auto& binding : entry.bindings)
        {
            if (glfwGetKey(m_glfwWindow, binding.key) == GLFW_PRESS)
            {
                held = true;
                break;
            }
        }

        entry.state.held = held;
        entry.state.pressed = held && !entry.previousHeld;
        entry.state.released = !held && entry.previousHeld;
        entry.previousHeld = held;
    }
}

void InputSystem::UpdateAxes()
{
    std::printf("[DEBUG_INPUT] UpdateAxes() called\n");
    
    float mouseDx = 0.0f;
    float mouseDy = 0.0f;
    if (m_window)
    {
        m_window->GetMouseDelta(mouseDx, mouseDy);
        std::printf("[DEBUG_INPUT] Mouse delta from window: dx=%.2f, dy=%.2f\n", mouseDx, mouseDy);
    }
    else
    {
        std::printf("[DEBUG_INPUT] WARNING: m_window is nullptr\n");
    }

    float scaledDx = mouseDx * m_mouseSettings.sensitivity;
    float scaledDy = mouseDy * m_mouseSettings.sensitivity;
    std::printf("[DEBUG_INPUT] Scaled mouse: dx=%.2f, dy=%.2f (sensitivity=%.2f)\n", 
                scaledDx, scaledDy, m_mouseSettings.sensitivity);

    if (m_mouseSettings.smoothing == MouseSettings::SmoothType::Ema)
    {
        if (!m_mouseInitialized)
        {
            m_mouseSmoothedX = scaledDx;
            m_mouseSmoothedY = scaledDy;
            m_mouseInitialized = true;
            std::printf("[DEBUG_INPUT] Mouse smoothing initialized (EMA)\n");
        }
        else
        {
            const float alpha = m_mouseSettings.alpha;
            const float invAlpha = 1.0f - alpha;
            m_mouseSmoothedX = alpha * scaledDx + invAlpha * m_mouseSmoothedX;
            m_mouseSmoothedY = alpha * scaledDy + invAlpha * m_mouseSmoothedY;
        }
        scaledDx = m_mouseSmoothedX;
        scaledDy = m_mouseSmoothedY;
    }
    else
    {
        m_mouseSmoothedX = scaledDx;
        m_mouseSmoothedY = scaledDy;
        m_mouseInitialized = true;
    }

    std::printf("[DEBUG_INPUT] Processing %zu axes\n", m_axes.size());
    
    for (auto& [axisName, entry] : m_axes)
    {
        float value = 0.0f;
        std::printf("[DEBUG_INPUT] Axis '%s': %zu bindings\n", axisName.c_str(), entry.bindings.size());
        
        for (const auto& binding : entry.bindings)
        {
            switch (binding.type)
            {
            case AxisBinding::Type::Key:
                if (m_glfwWindow && glfwGetKey(m_glfwWindow, binding.key) == GLFW_PRESS)
                {
                    value += binding.scale;
                    std::printf("[DEBUG_INPUT]   Key binding: key=%d, scale=%.2f, NEW_VALUE=%.2f\n", 
                                binding.key, binding.scale, value);
                }
                break;
            case AxisBinding::Type::MouseDelta:
                if (binding.mouseAxis == AxisBinding::MouseAxis::DeltaX)
                {
                    value += scaledDx * binding.scale;
                    std::printf("[DEBUG_INPUT]   Mouse DeltaX binding: scaledDx=%.2f, scale=%.2f, NEW_VALUE=%.2f\n", 
                                scaledDx, binding.scale, value);
                }
                else
                {
                    value += scaledDy * binding.scale;
                    std::printf("[DEBUG_INPUT]   Mouse DeltaY binding: scaledDy=%.2f, scale=%.2f, NEW_VALUE=%.2f\n", 
                                scaledDy, binding.scale, value);
                }
                break;
            }
        }
        entry.value = std::clamp(value, -1.0f, 1.0f);
        std::printf("[DEBUG_INPUT] Axis '%s' final value: %.2f\n", axisName.c_str(), entry.value);
    }
}