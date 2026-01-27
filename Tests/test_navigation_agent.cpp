#include "catch_amalgamated.hpp"
#include "engine/NavigationSystem.h"
#include "engine/Scene.h"
#include "engine/Components.h"

using namespace Genesis::Engine;

TEST_CASE("Navigation agent advances toward target", "[navigation]") {
    Scene scene;
    auto gridEntity = scene.Registry().create();
    NavGridComponent nav;
    nav.width = 4;
    nav.height = 1;
    nav.cellSize = 1.0f;
    nav.originX = 0.0f;
    nav.originZ = 0.0f;
    nav.autoBakeColliders = false;
    scene.Registry().emplace<NavGridComponent>(gridEntity, nav);

    auto agentEntity = scene.Registry().create();
    Transform t;
    t.x = 0.1f;
    t.z = 0.1f;
    scene.Registry().emplace<Transform>(agentEntity, t);
    NavAgentComponent agent;
    agent.speed = 1.0f;
    agent.targetX = 3.1f;
    agent.targetZ = 0.1f;
    agent.hasTarget = true;
    scene.Registry().emplace<NavAgentComponent>(agentEntity, agent);

    NavigationSystem::UpdateAgents(scene, 0.5);

    auto& updated = scene.Registry().get<Transform>(agentEntity);
    REQUIRE(updated.x > 0.1f);
}
