#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"

#ifdef HAVE_WASM3
TEST_CASE("WasmRuntime: init and basic load", "[wasm]") {
    REQUIRE(Genesis::Engine::WasmRuntime::Init() == true);
    // Loading a non-existent file should fail gracefully
    REQUIRE(Genesis::Engine::WasmRuntime::LoadModule("nonexistent.wasm") == false);
}
#else
TEST_CASE("WasmRuntime: not available", "[wasm]") {
    SUCCEED("Wasm3 not available; skipping WASM tests");
}
#endif
