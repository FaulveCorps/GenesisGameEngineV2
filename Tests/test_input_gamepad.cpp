#define SDL_MAIN_HANDLED
#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/IInput.h"
#include <SDL.h>
#include <cmath>

TEST_CASE("SDLInput: controller button/axis events are detected", "[input][gamepad]") {
    // Try to initialize the GameController subsystem; if not available, skip.
    if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER) != 0) {
        if (SDL_Init(SDL_INIT_EVENTS | SDL_INIT_JOYSTICK) != 0) {
            WARN("SDL gamecontroller/joystick not available; skipping gamepad test");
            SDL_Quit();
            return;
        }
    }

    REQUIRE(Genesis::Engine::Init() == true);
    if (!Genesis::Engine::CreateInputSubsystem("sdl")) {
        WARN("SDLInput backend not available; skipping test.");
        SDL_Quit();
        return;
    }

    auto in = Genesis::Engine::GetInputSubsystem();
    REQUIRE(in != nullptr);

    // Simulate controller A button down on controller instance 0
    SDL_Event e{};
    e.type = SDL_CONTROLLERBUTTONDOWN;
    e.cbutton.which = 0; // instance id 0, creates placeholder if needed
    e.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    e.cbutton.state = SDL_PRESSED;
    SDL_PushEvent(&e);

    in->Update(0.016);

    REQUIRE(in->IsControllerConnected(0) == true);
    REQUIRE(in->WasControllerButtonPressed(0, SDL_CONTROLLER_BUTTON_A) == true);
    REQUIRE(in->IsControllerButtonDown(0, SDL_CONTROLLER_BUTTON_A) == true);

    // Release
    SDL_Event e2{};
    e2.type = SDL_CONTROLLERBUTTONUP;
    e2.cbutton.which = 0;
    e2.cbutton.button = SDL_CONTROLLER_BUTTON_A;
    e2.cbutton.state = SDL_RELEASED;
    SDL_PushEvent(&e2);

    in->Update(0.016);
    REQUIRE(in->WasControllerButtonReleased(0, SDL_CONTROLLER_BUTTON_A) == true);
    REQUIRE(in->IsControllerButtonDown(0, SDL_CONTROLLER_BUTTON_A) == false);

    // Axis motion
    SDL_Event a{};
    a.type = SDL_CONTROLLERAXISMOTION;
    a.caxis.which = 0;
    a.caxis.axis = SDL_CONTROLLER_AXIS_LEFTX;
    a.caxis.value = 16000; // mid-right
    SDL_PushEvent(&a);

    in->Update(0.016);
    float axisVal = in->GetControllerAxis(0, SDL_CONTROLLER_AXIS_LEFTX);
    REQUIRE(std::fabs(axisVal - (16000.0f / 32767.0f)) < 0.06f);

    // Cleanup: switch back to null input
    Genesis::Engine::CreateInputSubsystem("null");
    SDL_Quit();
}
