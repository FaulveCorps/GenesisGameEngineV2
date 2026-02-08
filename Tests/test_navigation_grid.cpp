#include "catch_amalgamated.hpp"
#include "engine/NavigationSystem.h"
#include "engine/Scene.h"
#include "engine/Components.h"

using namespace Genesis::Engine;

TEST_CASE("Navigation grid: box collider blocks cells", "[navigation]") {
    Scene scene;
    auto gridEntity = scene.Registry().create();
    NavGridComponent nav;
    nav.width = 5;
    nav.height = 1;
    nav.cellSize = 1.0f;
    nav.originX = 0.0f;
    nav.originZ = 0.0f;
    nav.autoBakeColliders = true;
    scene.Registry().emplace<NavGridComponent>(gridEntity, nav);

    auto boxEntity = scene.Registry().create();
    BoxColliderComponent box;
    box.size[0] = 1.0f;
    box.size[1] = 1.0f;
    box.size[2] = 1.0f;
    scene.Registry().emplace<BoxColliderComponent>(boxEntity, box);
    Transform t;
    t.x = 1.0f;
    t.z = 0.0f;
    scene.Registry().emplace<Transform>(boxEntity, t);

    std::vector<uint8_t> blocked;
    auto graph = NavigationSystem::BuildGrid(scene, nav, &blocked);

    REQUIRE(graph.IsWalkable({1, 0}) == false);
    REQUIRE(blocked.size() == static_cast<size_t>(nav.width * nav.height));
}

TEST_CASE("Navigation grid: find path in world space", "[navigation]") {
    Scene scene;
    NavGridComponent nav;
    nav.width = 4;
    nav.height = 2;
    nav.cellSize = 1.0f;
    nav.originX = 0.0f;
    nav.originZ = 0.0f;
    nav.autoBakeColliders = false;

    auto result = NavigationSystem::FindPath(scene, nav, 0.1f, 0.1f, 3.1f, 0.1f);
    REQUIRE(result.success == true);
    REQUIRE(!result.gridPath.empty());
    REQUIRE(result.worldPath.size() == result.gridPath.size() * 3);
}

TEST_CASE("Navigation grid: sphere collider blocks cells", "[navigation]") {
    Scene scene;
    NavGridComponent nav;
    nav.width = 5;
    nav.height = 1;
    nav.cellSize = 1.0f;
    nav.originX = 0.0f;
    nav.originZ = 0.0f;
    nav.autoBakeColliders = true;

    auto gridEntity = scene.Registry().create();
    scene.Registry().emplace<NavGridComponent>(gridEntity, nav);

    auto sphereEntity = scene.Registry().create();
    SphereColliderComponent sphere;
    sphere.radius = 0.6f;
    sphere.offset[0] = 0.0f;
    sphere.offset[1] = 0.0f;
    sphere.offset[2] = 0.0f;
    scene.Registry().emplace<SphereColliderComponent>(sphereEntity, sphere);

    Transform t;
    t.x = 2.0f;
    t.z = 0.0f;
    scene.Registry().emplace<Transform>(sphereEntity, t);

    std::vector<uint8_t> blocked;
    auto graph = NavigationSystem::BuildGrid(scene, nav, &blocked);

    REQUIRE(graph.IsWalkable({2, 0}) == false);
    REQUIRE(blocked.size() == static_cast<size_t>(nav.width * nav.height));
}
