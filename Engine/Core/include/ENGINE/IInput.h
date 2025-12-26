#pragma once

#include "engine/ISubsystem.h"
#include <cstdint>

namespace Genesis::Engine {

class IInput : public ISubsystem {
public:
    virtual ~IInput() = default;

    // Update internal state (should be called once per frame after event polling)
    virtual void Update(double dt) = 0;

    // Keyboard queries use SDL scancode integers for now (SDL_SCANCODE_*). Returns true if the key is currently down.
    virtual bool IsKeyDown(int scancode) const = 0;
    // Edge queries
    virtual bool WasKeyPressed(int scancode) const = 0;
    virtual bool WasKeyReleased(int scancode) const = 0;

    // Mouse
    virtual void GetMousePosition(int& x, int& y) const = 0;
    virtual bool IsMouseButtonDown(int button) const = 0;
    virtual bool WasMouseButtonPressed(int button) const = 0;
    virtual bool WasMouseButtonReleased(int button) const = 0;
};

} // namespace Genesis::Engine
