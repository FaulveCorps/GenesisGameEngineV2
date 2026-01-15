#include "engine/EditorHelpers.h"
#include "engine/Engine.h"
#include "engine/IAudio.h"
#include "engine/Components.h"
#include <filesystem>
#include <algorithm>
#include <unordered_map>
#include <mutex>
#include <chrono>

namespace Genesis::Engine {

static bool IsAudioExtension(const std::string& ext) {
    std::string e = ext;
    std::transform(e.begin(), e.end(), e.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return (e == ".wav" || e == ".ogg" || e == ".mp3" || e == ".flac");
}

bool AssignContentItemToAudioComponent(entt::registry& registry, entt::entity entity, const std::string& path) {
    std::filesystem::path p(path);
    std::string ext = p.extension().string();
    if (!IsAudioExtension(ext)) return false;

    AudioComponent ac;
    ac.soundPath = path;
    // Keep default values for other fields
    registry.emplace_or_replace<AudioComponent>(entity, ac);
    return true;
}

bool PlayAudioPreview(const std::string& path, float volume) {
    if (path.empty()) return false;
    auto a = GetAudioSubsystem();
    if (!a) return false;
    return a->PlayOneShot(path, volume);
}

// Editor-only simple particle preview state. Keyed by (registry pointer, entity).
static std::unordered_map<uint64_t, std::chrono::steady_clock::time_point> g_particlePreviews;
static std::mutex g_particleMutex;

static uint64_t PreviewKey(entt::registry& registry, entt::entity e) {
    // Combine registry pointer and entity value into a 64-bit key.
    uint64_t r = reinterpret_cast<uint64_t>(&registry);
    uint64_t ent = static_cast<uint64_t>(static_cast<uint32_t>(e));
    return (r ^ (ent << 32));
}

bool StartParticlePreview(entt::registry& registry, entt::entity entity) {
    if (!registry.valid(entity)) return false;
    if (!registry.all_of<ParticleSystemComponent>(entity)) return false;
    std::lock_guard<std::mutex> lk(g_particleMutex);
    g_particlePreviews[PreviewKey(registry, entity)] = std::chrono::steady_clock::now();
    return true;
}

bool StopParticlePreview(entt::registry& registry, entt::entity entity) {
    std::lock_guard<std::mutex> lk(g_particleMutex);
    auto it = g_particlePreviews.find(PreviewKey(registry, entity));
    if (it == g_particlePreviews.end()) return false;
    g_particlePreviews.erase(it);
    return true;
}

bool IsParticlePreviewPlaying(entt::registry& registry, entt::entity entity) {
    std::lock_guard<std::mutex> lk(g_particleMutex);
    return g_particlePreviews.find(PreviewKey(registry, entity)) != g_particlePreviews.end();
}

} // namespace Genesis::Engine
