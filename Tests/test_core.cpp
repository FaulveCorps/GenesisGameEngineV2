#define CATCH_CONFIG_MAIN
#include <catch_amalgamated.hpp>

#include "engine/Engine.h"
#include "engine/Model.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#include "engine/PluginManager.h"

#include <filesystem>
#include <memory>

using namespace Genesis::Engine;
namespace fs = std::filesystem;

TEST_CASE("Engine Init/Shutdown") {
    REQUIRE(Genesis::Engine::Init("") == true);
    Genesis::Engine::Shutdown();
}

TEST_CASE("Model can load triangle asset") {
    std::string assetPath = std::string(PROJECT_SOURCE_DIR) + "/Assets/models/triangle.obj";
    Model m;
    REQUIRE(m.Load(assetPath) == true);
}

TEST_CASE("Scene registry can create entity and components") {
    Scene scene;
    auto &r = scene.Registry();
    auto e = r.create();
    r.emplace<Transform>(e, Transform{});
    REQUIRE(r.any_of<Transform>(e));
}

TEST_CASE("PluginManager fails to load missing plugin gracefully") {
    PluginManager pm;
    REQUIRE(pm.LoadPlugin("nonexistent_plugin_file.so") == false);
}
