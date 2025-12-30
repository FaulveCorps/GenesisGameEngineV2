#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include "engine/IPhysics.h"
#include "engine/Wasm/ResourceLimits.h"
#include <atomic>

#ifdef HAVE_WASM3
#include "wasm3.h"
#endif

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
    // Call exported function with a timeout (ms). If the call does not complete within timeoutMs,
    // the module is marked as timed out and the call returns false.
    static bool CallExportedWithTimeout(const std::string& moduleName, const std::string& funcName, const std::vector<std::string>& args, uint32_t timeoutMs);
    // Set default resource limits for runtime (affects future calls / modules)
    static void SetDefaultResourceLimits(const ResourceLimits& limits);

    // Return pointer to the module's timed_out flag, or nullptr if not managed by WasmRuntime.
    // This pointer is intentionally a raw pointer so trampolines can perform a fast, lock-free check.
    static std::atomic<bool>* GetModuleTimedOutPtr(IM3Module module);

    // Return the per-module memory limit in bytes. If the module is not managed by the runtime,
    // the runtime's default memory limit will be returned.
    static std::size_t GetModuleMemoryLimitBytes(IM3Module module);

    // Return configured per-module execution timeout in milliseconds. Falls back to runtime default when module not found.
    static uint32_t GetModuleExecutionTimeoutMs(IM3Module module);

    // For physics: install callback forwarders that invoke module exports named 'on_contact_begin'/'on_contact_end'
    static void RegisterPhysicsCallbacks(std::shared_ptr<IPhysics> phys);

    // Simple helper: enumerate loaded modules
    static std::vector<std::string> LoadedModules();

    #ifdef HAVE_WASM3

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
