#include "engine/Animation.h"
#include "engine/Scene.h"
#include "engine/Components.h"

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/euler_angles.hpp>
#include <algorithm>
#include <cmath>

namespace Genesis::Engine {

void AnimationSystem::ApplyPose(const AnimationClip& clip, float time, Transform& transform) {
    if (clip.channels.empty()) return;

    const auto& channel = clip.channels[0];
    if (channel.keyframes.empty()) return;

    size_t nextIndex = 0;
    for (; nextIndex < channel.keyframes.size(); ++nextIndex) {
        if (channel.keyframes[nextIndex].time > time) break;
    }

    size_t prevIndex = (nextIndex == 0) ? 0 : nextIndex - 1;
    nextIndex = std::min(nextIndex, channel.keyframes.size() - 1);

    const auto& prevKey = channel.keyframes[prevIndex];
    const auto& nextKey = channel.keyframes[nextIndex];

    float factor = 0.0f;
    float timeDiff = nextKey.time - prevKey.time;
    if (timeDiff > 0.0001f) {
        factor = (time - prevKey.time) / timeDiff;
    }
    factor = std::clamp(factor, 0.0f, 1.0f);

    glm::vec3 pos = glm::mix(prevKey.position, nextKey.position, factor);
    glm::quat rot = glm::slerp(prevKey.rotation, nextKey.rotation, factor);
    glm::vec3 scale = glm::mix(prevKey.scale, nextKey.scale, factor);

    transform.x = pos.x;
    transform.y = pos.y;
    transform.z = pos.z;

    glm::vec3 euler = glm::eulerAngles(rot);
    transform.rx = euler.x;
    transform.ry = euler.y;
    transform.rz = euler.z;

    transform.sx = scale.x;
    transform.sy = scale.y;
    transform.sz = scale.z;
}

void AnimationSystem::Update(Scene& scene, double dt, bool editorPreview) {
    auto view = scene.Registry().view<AnimationComponent, Transform>();
    
    view.each([&](auto entity, auto& anim, auto& transform) {
        if (!anim.clip) return;

        if (editorPreview) {
            if (!anim.previewInEditor) return;
            if (anim.isPlaying) {
                anim.currentTime += (float)dt * anim.speed;
            }
        } else {
            if (!anim.isPlaying) return;
            anim.currentTime += (float)dt * anim.speed;
        }

        if (anim.loop) {
            if (anim.clip->duration > 0.0f)
                anim.currentTime = std::fmod(anim.currentTime, anim.clip->duration);
        } else {
            if (anim.currentTime > anim.clip->duration) {
                anim.currentTime = anim.clip->duration;
                anim.isPlaying = false;
            }
        }

        ApplyPose(*anim.clip, anim.currentTime, transform);
    });
}

} // namespace Genesis::Engine
