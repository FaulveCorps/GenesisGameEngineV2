#pragma once

#include "engine/ISubsystem.h"
#include <cstdint>
#include <string>

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

    // Controller / Gamepad (SDL_GameController mapping)
    virtual int GetControllerCount() const = 0;
    virtual bool IsControllerConnected(int controllerId) const = 0;
    virtual bool IsControllerButtonDown(int controllerId, int button) const = 0;
    virtual bool WasControllerButtonPressed(int controllerId, int button) const = 0;
    virtual bool WasControllerButtonReleased(int controllerId, int button) const = 0;
    virtual float GetControllerAxis(int controllerId, int axis) const = 0; // returns -1..1
    virtual std::string GetControllerName(int controllerId) const = 0;
};

} // namespace Genesis::Engine
