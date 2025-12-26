#include "engine/IInput.h"
#include "engine/SubsystemRegistry.h"
#include <SDL.h>
#include <vector>
#include <mutex>

namespace Genesis::Engine {

class SDLInput : public IInput {
public:
    SDLInput() {}
    bool Init() override { return true; }
    void Shutdown() override {}
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

private:
    mutable std::mutex m_lock;
    std::vector<Uint8> m_prevKeys;
    std::vector<Uint8> m_currKeys;
    int m_numKeys = 0;
    Uint32 m_prevMouseButtons = 0;
    Uint32 m_currMouseButtons = 0;
    int m_mouseX = 0;
    int m_mouseY = 0;
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
