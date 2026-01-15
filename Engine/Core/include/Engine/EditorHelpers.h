#pragma once

#include <string>
#include <entt/entt.hpp>

namespace Genesis::Engine {

// Assign a content browser item path to an AudioComponent on the entity.
// Returns true if assignment was performed (extension recognized and component created/updated).
bool AssignContentItemToAudioComponent(entt::registry& registry, entt::entity entity, const std::string& path);

// Play a one-shot audio preview using the active audio subsystem (if any).
// Returns true if PlayOneShot was requested successfully (backend present and path non-empty).
bool PlayAudioPreview(const std::string& path, float volume = 1.0f);

// Particle preview controls (editor-only). These functions are safe to call from tests.
bool StartParticlePreview(entt::registry& registry, entt::entity entity);
bool StopParticlePreview(entt::registry& registry, entt::entity entity);
bool IsParticlePreviewPlaying(entt::registry& registry, entt::entity entity);

} // namespace Genesis::Engine
