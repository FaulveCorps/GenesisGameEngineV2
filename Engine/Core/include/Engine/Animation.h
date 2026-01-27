#pragma once

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace Genesis::Engine {

struct Keyframe {
    float time;
    glm::vec3 position;
    glm::quat rotation;
    glm::vec3 scale;
};

struct AnimationChannel {
    std::string nodeName; // Or bone ID
    std::vector<Keyframe> keyframes;
};

struct AnimationClip {
    std::string name;
    float duration;
    std::vector<AnimationChannel> channels;
};

struct AnimationComponent {
    std::shared_ptr<AnimationClip> clip;
    float currentTime = 0.0f;
    float speed = 1.0f;
    bool loop = true;
    bool isPlaying = true;
    bool previewInEditor = false;
};

class Scene;

class AnimationSystem {
public:
    static void Update(Scene& scene, double dt, bool editorPreview = false);
    static void ApplyPose(const AnimationClip& clip, float time, Transform& transform);
};

} // namespace Genesis::Engine
