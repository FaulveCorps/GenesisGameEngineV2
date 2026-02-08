#include <catch2/catch_all.hpp>
#include "engine/Animation.h"
#include "engine/Scene.h"
#include "engine/Components.h"
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

using namespace Genesis::Engine;

TEST_CASE("AnimationSystem Smoke Test", "[animation]") {
    Scene scene;
    auto entity = scene.Registry().create();
    
    Transform t;
    t.x = 0.0f;
    scene.Registry().emplace<Transform>(entity, t);

    auto clip = std::make_shared<AnimationClip>();
    clip->duration = 1.0f;
    
    AnimationChannel channel;
    channel.nodeName = "Root";
    
    Keyframe k1;
    k1.time = 0.0f;
    k1.position = glm::vec3(0.0f, 0.0f, 0.0f);
    k1.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    k1.scale = glm::vec3(1.0f);
    
    Keyframe k2;
    k2.time = 1.0f;
    k2.position = glm::vec3(10.0f, 0.0f, 0.0f);
    k2.rotation = glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    k2.scale = glm::vec3(1.0f);
    
    channel.keyframes.push_back(k1);
    channel.keyframes.push_back(k2);
    clip->channels.push_back(channel);

    AnimationComponent anim;
    anim.clip = clip;
    anim.isPlaying = true;
    anim.loop = false;
    scene.Registry().emplace<AnimationComponent>(entity, anim);

    // Update 0.5s
    AnimationSystem::Update(scene, 0.5);
    
    auto& t_updated = scene.Registry().get<Transform>(entity);
    REQUIRE(t_updated.x == Catch::Approx(5.0f));
    
    // Update another 0.5s
    AnimationSystem::Update(scene, 0.5);
    auto& t_final = scene.Registry().get<Transform>(entity);
    REQUIRE(t_final.x == Catch::Approx(10.0f));
}
