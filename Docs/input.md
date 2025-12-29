# Input Subsystem (IInput)

Overview
--------
The Input subsystem provides a simple, backend-agnostic API for keyboard and mouse queries. It follows the Subsystem pattern used across the engine and supports multiple backends ("null" and "sdl" are provided).

Key points
----------
- Interface: `IInput` (see `Engine/Core/include/Engine/IInput.h`) provides:
  - `Update(double dt)` — update per-frame state (call once per frame after event polling)
  - `IsKeyDown(int scancode)` — current key state
  - `WasKeyPressed(int scancode)` / `WasKeyReleased(int scancode)` — edge detection
  - Mouse queries: `GetMousePosition`, `IsMouseButtonDown`, `WasMouseButtonPressed`, `WasMouseButtonReleased`
- Scancodes use SDL scancode integers (`SDL_SCANCODE_*`).
- Controller/Gamepad support (SDL_GameController): `GetControllerCount`, `IsControllerConnected`, `IsControllerButtonDown`, `WasControllerButtonPressed`, `WasControllerButtonReleased`, `GetControllerAxis`, and `GetControllerName`.

Usage
-----
1. Initialize engine and window (SDL must be initialized before creating the `sdl` backend).
2. Optionally switch to the SDL backend:

```cpp
// after window.Init(...)
if (!Genesis::Engine::CreateInputSubsystem("sdl")) {
    std::cout << "SDL input subsystem not available; using null input" << std::endl;
}
```

3. Each frame (after `Window::PollEvents()`), call `Update()` and then query input:

```cpp
if (auto in = Genesis::Engine::GetInputSubsystem()) in->Update(dt);
if (in && (in->WasKeyPressed(SDL_SCANCODE_F2) || in->WasControllerButtonPressed(0, SDL_CONTROLLER_BUTTON_A))) { /* handle edge */ }
int mx, my; in->GetMousePosition(mx, my);
```

Notes & Testing
----------------
- The `NullInput` backend is always available and is used as the default in `Engine::Init()`.
- `SDLInput` is registered if SDL is available and can be selected at runtime.
- A unit test `Tests/test_input.cpp` verifies the null backend and failure case for unknown backends.

Extending
---------
To add another backend, implement `IInput`, register a factory via `SubsystemRegistry::Instance().RegisterFactory("Input", "yourname", [](){ return std::make_unique<YourInput>(); });` and call `CreateInputSubsystem("yourname")` at runtime.
