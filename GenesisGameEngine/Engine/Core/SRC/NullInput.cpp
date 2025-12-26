#include "engine/IInput.h"
#include "engine/SubsystemRegistry.h"

#include <iostream>

namespace Genesis::Engine {

class NullInput : public IInput {
public:
    bool Init() override { std::cout << "NullInput: Init\n"; return true; }
    void Shutdown() override { std::cout << "NullInput: Shutdown\n"; }
    void Update(double /*dt*/) override {}
    std::string Name() const override { return "null"; }

    bool IsKeyDown(int /*scancode*/) const override { return false; }
    bool WasKeyPressed(int /*scancode*/) const override { return false; }
    bool WasKeyReleased(int /*scancode*/) const override { return false; }

    void GetMousePosition(int& x, int& y) const override { x = 0; y = 0; }
    bool IsMouseButtonDown(int /*button*/) const override { return false; }
    bool WasMouseButtonPressed(int /*button*/) const override { return false; }
    bool WasMouseButtonReleased(int /*button*/) const override { return false; }

    // Controller no-op
    int GetControllerCount() const override { return 0; }
    bool IsControllerConnected(int /*controllerId*/) const override { return false; }
    bool IsControllerButtonDown(int /*controllerId*/, int /*button*/) const override { return false; }
    bool WasControllerButtonPressed(int /*controllerId*/, int /*button*/) const override { return false; }
    bool WasControllerButtonReleased(int /*controllerId*/, int /*button*/) const override { return false; }
    float GetControllerAxis(int /*controllerId*/, int /*axis*/) const override { return 0.0f; }
    std::string GetControllerName(int /*controllerId*/) const override { return std::string(); }
};

static bool register_null_input = []() {
    SubsystemRegistry::Instance().RegisterFactory("Input", "null", []() {
        return std::make_unique<NullInput>();
    });
    return true;
}();

void RegisterNullInputFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Input", "null", []() {
        return std::make_unique<NullInput>();
    });
}

} // namespace Genesis::Engine
