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
#endif
