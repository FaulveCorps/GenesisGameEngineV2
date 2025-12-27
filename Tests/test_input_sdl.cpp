#define SDL_MAIN_HANDLED
#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/IInput.h"
#include <SDL.h>

TEST_CASE("SDLInput: press/release events are detected", "[input][sdl]") {
    // Initialize SDL event subsystem (or skip if not available)
    if (SDL_Init(SDL_INIT_EVENTS) != 0) {
        if (SDL_Init(SDL_INIT_VIDEO) != 0) {
            WARN("SDL not available; skipping SDLInput event test.");
            SDL_Quit();
            return;
        }
    }

    // Ensure engine and factories are registered
    REQUIRE(Genesis::Engine::Init() == true);

    if (!Genesis::Engine::CreateInputSubsystem("sdl")) {
        WARN("SDLInput backend not available; skipping test.");
        SDL_Quit();
        return;
    }

    auto in = Genesis::Engine::GetInputSubsystem();
    REQUIRE(in != nullptr);

    // Simulate key down for 'A'
    SDL_Event e{};
    e.type = SDL_KEYDOWN;
    e.key.keysym.scancode = SDL_SCANCODE_A;
    e.key.keysym.sym = SDLK_a;
    e.key.state = SDL_PRESSED;
    SDL_PushEvent(&e);

    in->Update(0.016);

    REQUIRE(in->WasKeyPressed(SDL_SCANCODE_A) == true);
    REQUIRE(in->IsKeyDown(SDL_SCANCODE_A) == true);

    // Simulate key up
    SDL_Event e2{};
    e2.type = SDL_KEYUP;
    e2.key.keysym.scancode = SDL_SCANCODE_A;
    e2.key.keysym.sym = SDLK_a;
    e2.key.state = SDL_RELEASED;
    SDL_PushEvent(&e2);

    in->Update(0.016);
    REQUIRE(in->WasKeyReleased(SDL_SCANCODE_A) == true);
    REQUIRE(in->IsKeyDown(SDL_SCANCODE_A) == false);

    // Mouse button press
    SDL_Event me{};
    me.type = SDL_MOUSEBUTTONDOWN;
    me.button.button = SDL_BUTTON_LEFT;
    me.button.x = 10; me.button.y = 20;
    SDL_PushEvent(&me);

    in->Update(0.016);
    REQUIRE(in->WasMouseButtonPressed(SDL_BUTTON_LEFT) == true);
    REQUIRE(in->IsMouseButtonDown(SDL_BUTTON_LEFT) == true);
    int mx, my; in->GetMousePosition(mx, my);
    REQUIRE(mx == 10);
    REQUIRE(my == 20);

    // Mouse release
    SDL_Event me2{};
    me2.type = SDL_MOUSEBUTTONUP;
    me2.button.button = SDL_BUTTON_LEFT;
    me2.button.x = 11; me2.button.y = 22;
    SDL_PushEvent(&me2);

    in->Update(0.016);
    REQUIRE(in->WasMouseButtonReleased(SDL_BUTTON_LEFT) == true);
    REQUIRE(in->IsMouseButtonDown(SDL_BUTTON_LEFT) == false);

    // Cleanup
    Genesis::Engine::CreateInputSubsystem("null");
    SDL_Quit();
}
