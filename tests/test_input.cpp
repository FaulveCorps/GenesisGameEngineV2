#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/IInput.h"

TEST_CASE("InputSubsystem: Create null subsystem and default behavior", "[input]") {
    // Ensure engine init registers factories and creates default null input
    REQUIRE(Genesis::Engine::Init() == true);

    // Create null input explicitly (should succeed)
    REQUIRE(Genesis::Engine::CreateInputSubsystem("null") == true);
    auto input = Genesis::Engine::GetInputSubsystem();
    REQUIRE(input != nullptr);

    // Update and query defaults
    input->Update(0.016);
    REQUIRE(!input->IsKeyDown(0));
    REQUIRE(!input->WasKeyPressed(0));
    REQUIRE(!input->WasKeyReleased(0));

    int x = 1, y = 2;
    input->GetMousePosition(x, y);
    REQUIRE(x == 0);
    REQUIRE(y == 0);
    REQUIRE(!input->IsMouseButtonDown(1));
    REQUIRE(!input->WasMouseButtonPressed(1));
    REQUIRE(!input->WasMouseButtonReleased(1));
}

TEST_CASE("InputSubsystem: Create invalid backend fails", "[input]") {
    REQUIRE(!Genesis::Engine::CreateInputSubsystem("nope"));
}
