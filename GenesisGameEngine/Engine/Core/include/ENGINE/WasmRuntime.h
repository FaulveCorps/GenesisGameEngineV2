#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "engine/IPhysics.h"

namespace Genesis::Engine {

class WasmRuntime {
public:
    // Initialize runtime (called on demand)
    static bool Init();
    static void Shutdown();

    // Load a module from a wasm file; returns true on success
    static bool LoadModule(const std::filesystem::path& modulePath);

    // Load a module from raw wasm bytes (useful for tests and embedded modules)
    static bool LoadModuleFromBytes(const std::string& moduleName, const std::vector<uint8_t>& bytes);

    // Call exported function by name with optional string args (uses runtime's CallArgv mechanism)
    static bool CallExported(const std::string& moduleName, const std::string& funcName, const std::vector<std::string>& args = {});

    // For physics: install callback forwarders that invoke module exports named 'on_contact_begin'/'on_contact_end'
    static void RegisterPhysicsCallbacks(std::shared_ptr<IPhysics> phys);

    // Simple helper: enumerate loaded modules
    static std::vector<std::string> LoadedModules();
};

} // namespace Genesis::Engine
