#include "engine/IInput.h"
#include "engine/SubsystemRegistry.h"
#include <SDL.h>
#include <vector>
#include <mutex>
#include <unordered_map>
#include <string>
#include <algorithm>

namespace Genesis::Engine {

class SDLInput : public IInput {
public:
    SDLInput() {}
    bool Init() override { return true; }
    void Shutdown() override {
        std::lock_guard<std::mutex> lock(m_lock);
        for (auto &c : m_controllers) {
            if (c.controller) { SDL_GameControllerClose(c.controller); c.controller = nullptr; }
        }
    }
    std::string Name() const override { return "sdl"; }

    void Update(double /*dt*/) override {
        // Ensure SDL's key state is up to date
        SDL_PumpEvents();
        // Read system key state
        const Uint8* keyState = SDL_GetKeyboardState(&m_numKeys);
        std::lock_guard<std::mutex> lock(m_lock);
        // Initialize previous keys if necessary, otherwise slide previous <- current
        if (m_prevKeys.size() != static_cast<size_t>(m_numKeys)) {
            m_prevKeys.assign(keyState, keyState + m_numKeys);
        } else {
            m_prevKeys = m_currKeys;
        }
        m_currKeys.assign(keyState, keyState + m_numKeys);

        // Shift controller previous state <- current for all known controllers
        for (auto &c : m_controllers) {
            c.prevButtons = c.currButtons;
            c.prevAxes = c.currAxes;
        }

        // Process queued SDL events so tests can simulate input via SDL_PushEvent
        SDL_Event ev;
        bool mouseEventProcessed = false;
        while (SDL_PollEvent(&ev)) {
            switch (ev.type) {
            case SDL_KEYDOWN:
            case SDL_KEYUP: {
                int sc = ev.key.keysym.scancode;
                if (sc >= 0 && sc < m_numKeys) {
                    m_currKeys[sc] = (ev.type == SDL_KEYDOWN) ? 1 : 0;
                }
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            case SDL_MOUSEBUTTONUP: {
                mouseEventProcessed = true;
                m_prevMouseButtons = m_currMouseButtons;
                if (ev.type == SDL_MOUSEBUTTONDOWN)
                    m_currMouseButtons |= SDL_BUTTON(ev.button.button);
                else
                    m_currMouseButtons &= ~SDL_BUTTON(ev.button.button);
                m_mouseX = ev.button.x;
                m_mouseY = ev.button.y;
                break;
            }
            case SDL_MOUSEMOTION: {
                mouseEventProcessed = true;
                m_mouseX = ev.motion.x;
                m_mouseY = ev.motion.y;
                break;
            }
            case SDL_CONTROLLERDEVICEADDED: {
                // 'which' is the device index - attempt to open
                int device_index = ev.cdevice.which;
                SDL_GameController* gc = SDL_GameControllerOpen(device_index);
                if (gc) {
                    Sint32 instanceId = SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(gc));
                    size_t idx = ensureControllerIndex(instanceId);
                    m_controllers[idx].controller = gc;
                    const char* nm = SDL_GameControllerName(gc);
                    m_controllers[idx].name = nm ? nm : std::string();
                }
                break;
            }
            case SDL_CONTROLLERDEVICEREMOVED: {
                // which is the joystick instance id
                Sint32 instanceId = ev.cdevice.which;
                auto it = m_instanceIdToIndex.find(instanceId);
                if (it != m_instanceIdToIndex.end()) {
                    size_t idx = it->second;
                    if (m_controllers[idx].controller) {
                        SDL_GameControllerClose(m_controllers[idx].controller);
                        m_controllers[idx].controller = nullptr;
                    }
                    // remove by swapping last element into this slot
                    size_t last = m_controllers.size() - 1;
                    if (idx != last) {
                        Controller moved = std::move(m_controllers[last]);
                        m_controllers[idx] = std::move(moved);
                        m_instanceIdToIndex[m_controllers[idx].instanceId] = idx;
                    }
                    m_instanceIdToIndex.erase(it);
                    m_controllers.pop_back();
                }
                break;
            }
            case SDL_CONTROLLERBUTTONDOWN:
            case SDL_CONTROLLERBUTTONUP: {
                Sint32 instanceId = ev.cbutton.which;
                size_t idx = ensureControllerIndex(instanceId);
                int btn = ev.cbutton.button;
                if (btn >= 0 && btn < SDL_CONTROLLER_BUTTON_MAX) {
                    m_controllers[idx].currButtons[btn] = (ev.type == SDL_CONTROLLERBUTTONDOWN) ? 1 : 0;
                }
                break;
            }
            case SDL_CONTROLLERAXISMOTION: {
                Sint32 instanceId = ev.caxis.which;
                size_t idx = ensureControllerIndex(instanceId);
                int axis = ev.caxis.axis;
                if (axis >= 0 && axis < SDL_CONTROLLER_AXIS_MAX) {
                    m_controllers[idx].currAxes[axis] = ev.caxis.value;
                }
                break;
            }
            default:
                // Leave other events (window events etc) to Window::PollEvents()
                break;
            }
        }

        // Fallback: also query mouse state, but only if no mouse event processed this frame
        int mx, my; Uint32 ms = SDL_GetMouseState(&mx, &my);
        if (!mouseEventProcessed && ms != m_currMouseButtons) {
            m_prevMouseButtons = m_currMouseButtons;
            m_currMouseButtons = ms;
        }
        // Only update mouse position from system if we didn't process a mouse event
        if (!mouseEventProcessed) {
            m_mouseX = mx; m_mouseY = my;
        }
    }

    bool IsKeyDown(int scancode) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (scancode < 0 || scancode >= m_numKeys) return false;
        return m_currKeys[scancode];
    }

    bool WasKeyPressed(int scancode) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (scancode < 0 || scancode >= m_numKeys) return false;
        return !m_prevKeys[scancode] && m_currKeys[scancode];
    }

    bool WasKeyReleased(int scancode) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (scancode < 0 || scancode >= m_numKeys) return false;
        return m_prevKeys[scancode] && !m_currKeys[scancode];
    }

    void GetMousePosition(int& x, int& y) const override {
        x = m_mouseX; y = m_mouseY;
    }

    bool IsMouseButtonDown(int button) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        return (m_currMouseButtons & SDL_BUTTON(button)) != 0;
    }

    bool WasMouseButtonPressed(int button) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        return !(m_prevMouseButtons & SDL_BUTTON(button)) && (m_currMouseButtons & SDL_BUTTON(button));
    }

    bool WasMouseButtonReleased(int button) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        return (m_prevMouseButtons & SDL_BUTTON(button)) && !(m_currMouseButtons & SDL_BUTTON(button));
    }

    // Controller APIs
    int GetControllerCount() const override {
        std::lock_guard<std::mutex> lock(m_lock);
        return static_cast<int>(m_controllers.size());
    }

    bool IsControllerConnected(int controllerId) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        return (controllerId >= 0 && controllerId < static_cast<int>(m_controllers.size()));
    }

    bool IsControllerButtonDown(int controllerId, int button) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (controllerId < 0 || controllerId >= static_cast<int>(m_controllers.size())) return false;
        if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) return false;
        return m_controllers[controllerId].currButtons[button];
    }

    bool WasControllerButtonPressed(int controllerId, int button) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (controllerId < 0 || controllerId >= static_cast<int>(m_controllers.size())) return false;
        if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) return false;
        const auto &c = m_controllers[controllerId];
        return !c.prevButtons[button] && c.currButtons[button];
    }

    bool WasControllerButtonReleased(int controllerId, int button) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (controllerId < 0 || controllerId >= static_cast<int>(m_controllers.size())) return false;
        if (button < 0 || button >= SDL_CONTROLLER_BUTTON_MAX) return false;
        const auto &c = m_controllers[controllerId];
        return c.prevButtons[button] && !c.currButtons[button];
    }

    float GetControllerAxis(int controllerId, int axis) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (controllerId < 0 || controllerId >= static_cast<int>(m_controllers.size())) return 0.0f;
        if (axis < 0 || axis >= SDL_CONTROLLER_AXIS_MAX) return 0.0f;
        int v = m_controllers[controllerId].currAxes[axis];
        float f = 0.0f;
        if (v >= 0) f = static_cast<float>(v) / 32767.0f;
        else f = static_cast<float>(v) / 32768.0f;
        f = std::max(-1.0f, std::min(1.0f, f));
        return f;
    }

    std::string GetControllerName(int controllerId) const override {
        std::lock_guard<std::mutex> lock(m_lock);
        if (controllerId < 0 || controllerId >= static_cast<int>(m_controllers.size())) return std::string();
        return m_controllers[controllerId].name;
    }

private:
    struct Controller {
        Sint32 instanceId = -1;
        SDL_GameController* controller = nullptr;
        std::vector<Uint8> prevButtons;
        std::vector<Uint8> currButtons;
        std::vector<int16_t> prevAxes;
        std::vector<int16_t> currAxes;
        std::string name;
    };

    size_t ensureControllerIndex(Sint32 instanceId) {
        auto it = m_instanceIdToIndex.find(instanceId);
        if (it != m_instanceIdToIndex.end()) return it->second;
        Controller c;
        c.instanceId = instanceId;
        c.prevButtons.assign(SDL_CONTROLLER_BUTTON_MAX, 0);
        c.currButtons.assign(SDL_CONTROLLER_BUTTON_MAX, 0);
        c.prevAxes.assign(SDL_CONTROLLER_AXIS_MAX, 0);
        c.currAxes.assign(SDL_CONTROLLER_AXIS_MAX, 0);
        size_t idx = m_controllers.size();
        m_controllers.push_back(std::move(c));
        m_instanceIdToIndex[instanceId] = idx;
        return idx;
    }

    mutable std::mutex m_lock;
    std::vector<Uint8> m_prevKeys;
    std::vector<Uint8> m_currKeys;
    int m_numKeys = 0;
    Uint32 m_prevMouseButtons = 0;
    Uint32 m_currMouseButtons = 0;
    int m_mouseX = 0;
    int m_mouseY = 0;

    // Controllers
    std::vector<Controller> m_controllers;
    std::unordered_map<Sint32, size_t> m_instanceIdToIndex;
};


static bool register_sdl_input = []() {
    SubsystemRegistry::Instance().RegisterFactory("Input", "sdl", []() {
        return std::make_unique<SDLInput>();
    });
    return true;
}();

void RegisterSDLInputFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Input", "sdl", []() {
        return std::make_unique<SDLInput>();
    });
}

} // namespace Genesis::Engine
