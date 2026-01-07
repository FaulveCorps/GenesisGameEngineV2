#include "engine/ScriptSystem.h"
#include "engine/Components.h"
#include "engine/Engine.h"
#include "engine/IScripting.h"
#include <iostream>

namespace Genesis::Engine {

void ScriptSystem::Update(Scene& scene, double dt, bool simulate) {
    auto scripting = GetScriptingSubsystem();
    if (!scripting) return;
    if (!simulate) return;

    auto view = scene.Registry().view<ScriptComponent>();
    for (auto entity : view) {
        auto& sc = view.get<ScriptComponent>(entity);
        
        // Check if script needs initialization
        if (!sc.initialized && !sc.scriptPath.empty()) {
            scripting->OnEntityScriptCreate((uint32_t)entity, sc.scriptPath);
            sc.initialized = true;
        }

        // Run update loop
        if (sc.initialized) {
            scripting->OnEntityScriptUpdate((uint32_t)entity, dt);
        }
    }
}

void ScriptSystem::OnDestroy(Scene& scene, entt::entity entity) {
    auto scripting = GetScriptingSubsystem();
    if (!scripting) return;

    if (scene.Registry().any_of<ScriptComponent>(entity)) {
        auto& sc = scene.Registry().get<ScriptComponent>(entity);
        if (sc.initialized) {
            scripting->OnEntityScriptDestroy((uint32_t)entity);
            sc.initialized = false;
        }
    }
}

} // namespace Genesis::Engine
