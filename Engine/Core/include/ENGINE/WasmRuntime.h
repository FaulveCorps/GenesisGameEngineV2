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

#ifdef HAVE_WASM3
#include "wasm3.h"

    // Small RAII token returned when registering a global host function. When the
    // token is destroyed the registration is removed (best-effort).
    struct HostBindingToken {
        HostBindingToken() = default;
        HostBindingToken(HostBindingToken&&) noexcept;
        HostBindingToken& operator=(HostBindingToken&&) noexcept;
        ~HostBindingToken() noexcept;
        bool valid() const noexcept;

    private:
        HostBindingToken(size_t id) : id(id) {}
        size_t id = SIZE_MAX;
        friend class WasmRuntime;
    };

    // Register a host function globally so that subsequent module loads will have
    // the import available. Returns a move-only token that unregisters on destroy.
    static HostBindingToken RegisterHostFunction(const std::string& ns, const std::string& name, const std::string& sig, M3RawCall cb);
#endif
};

} // namespace Genesis::Engine
