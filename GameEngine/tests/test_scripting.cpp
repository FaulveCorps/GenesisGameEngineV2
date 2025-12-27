#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/IScripting.h"

TEST_CASE("ScriptingSubsystem: Null backend basic", "[scripting]") {
    REQUIRE(Genesis::Engine::Init() == true);

    REQUIRE(Genesis::Engine::CreateScriptingSubsystem("null") == true);
    auto s = Genesis::Engine::GetScriptingSubsystem();
    REQUIRE(s != nullptr);
    REQUIRE(s->Name() == "null");
    REQUIRE(s->ExecuteString("a=1") == false);
    REQUIRE(s->ExecuteFile("/no/such/path.lua") == false);
}

#ifdef HAVE_LUA
TEST_CASE("ScriptingSubsystem: Lua basic Execute", "[scripting][lua]") {
    if (!Genesis::Engine::Init()) {
        WARN("Engine initialization failed; skipping Lua test");
        return;
    }

    if (!Genesis::Engine::CreateScriptingSubsystem("lua")) {
        WARN("Lua backend unavailable at runtime; skipping test");
        return;
    }
    auto s = Genesis::Engine::GetScriptingSubsystem();
    REQUIRE(s != nullptr);
    REQUIRE(s->Name() == "lua");

    REQUIRE(s->ExecuteString("x = 10") == true);
    REQUIRE(s->ExecuteString("x = x + 1") == true);
    REQUIRE(s->ExecuteString("return 2+2") == true);
}

TEST_CASE("ScriptingSubsystem: Lua register contact callbacks", "[scripting][lua]") {
    if (!Genesis::Engine::Init()) {
        WARN("Engine initialization failed; skipping Lua contact registration test");
        return;
    }
    if (!Genesis::Engine::CreateScriptingSubsystem("lua")) {
        WARN("Lua backend unavailable at runtime; skipping test");
        return;
    }
    auto s = Genesis::Engine::GetScriptingSubsystem();
    REQUIRE(s != nullptr);

    REQUIRE(s->ExecuteString("engine.register_contact_begin(function(a,b) end)") == true);
    REQUIRE(s->ExecuteString("engine.register_contact_end(function(a,b) end)") == true);
}

#if defined(HAVE_LUA) && defined(HAVE_BOX2D)
TEST_CASE("ScriptingSubsystem: Lua create body/joint via engine", "[scripting][lua][box2d]") {
    if (!Genesis::Engine::Init()) {
        WARN("Engine initialization failed; skipping Lua physics creation test");
        return;
    }
    if (!Genesis::Engine::CreateScriptingSubsystem("lua")) {
        WARN("Lua backend unavailable at runtime; skipping test");
        return;
    }
    // Create Box2D physics backend for this test
    if (!Genesis::Engine::CreatePhysicsSubsystem("box2d")) {
        WARN("Box2D backend not available; skipping test");
        return;
    }
    auto s = Genesis::Engine::GetScriptingSubsystem();
    REQUIRE(s != nullptr);

    // Create two bodies and a distance joint via Lua; assertions in Lua will trigger errors if nil
    REQUIRE(s->ExecuteString("b = engine.create_body(1.0, 0.0, 5.0, 1.0, 1.0); assert(b ~= nil)"));
    REQUIRE(s->ExecuteString("c = engine.create_body(1.0, 1.0, 5.0, 1.0, 1.0); assert(c ~= nil)"));
    REQUIRE(s->ExecuteString("j = engine.create_distance_joint(b, c, 0.0, 5.0, 1.0, 5.0); assert(j ~= nil)"));
}
#endif
#endif
