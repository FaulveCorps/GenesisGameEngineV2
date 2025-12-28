#include "engine/WasmRuntime.h"
#include "engine/SubsystemRegistry.h"
#include "engine/Engine.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <mutex>
#include <future>
#include <chrono>
#include <thread>
#include <atomic>
#include "engine/Wasm/ResourceLimits.h"



#include "wasm3.h"
#include "m3_env.h"


#ifdef HAVE_WASM3
#include "engine/WasmHostBindings.h"
#include <algorithm>
#endif

namespace Genesis::Engine {

#ifdef HAVE_WASM3
// Host implementations remain in the wasm-enabled section below

struct WasmModule {
    std::string name;
    std::vector<uint8_t> bytes;
    IM3Module module = nullptr;
    IM3Runtime runtime = nullptr;
    std::vector<HostBindings::Token> hostTokens;

    // Resource/monitoring state
    std::atomic<bool> timed_out{false};    // set when the module exceeded execution time
    std::atomic<int> active_calls{0};      // number of in-flight calls into this module


    // Ensure resources are freed when module is destroyed; noexcept to avoid terminating during stack unwinding
    ~WasmModule() noexcept {
        // Debug: trace destruction sequence
        std::cerr << "WasmModule::~WasmModule: destroying module='" << name << "' modulePtr=" << module << " runtime=" << runtime << std::endl;
        // Destroy host registration tokens first so they can unregister while module/runtime are still valid
        std::cerr << "WasmModule::~WasmModule: clearing " << hostTokens.size() << " tokens" << std::endl;
        hostTokens.clear();
        std::cerr << "WasmModule::~WasmModule: tokens cleared" << std::endl;
        // Clean up any global host callback registrations associated with this module so
        // holders are destroyed only after the module/runtime are gone.
        if (module) { HostBindings::CleanupModuleCallbacks(module); }

        if (runtime) {
            std::cerr << "WasmModule::~WasmModule: freeing runtime (will also free loaded module)=" << runtime << std::endl;
            m3_FreeRuntime(runtime);
            runtime = nullptr;
            module = nullptr; // runtime freed associated module
            std::cerr << "WasmModule::~WasmModule: runtime freed; module pointer cleared" << std::endl;
        } else if (module) {
            std::cerr << "WasmModule::~WasmModule: freeing modulePtr (not attached to runtime)=" << module << std::endl;
            m3_FreeModule(module);
            module = nullptr;
            std::cerr << "WasmModule::~WasmModule: module freed" << std::endl;
        }
    }
};

// Small RAII wrapper for IM3Environment
struct EnvRAII {
    IM3Environment env = nullptr;
    EnvRAII() noexcept = default;
    explicit EnvRAII(IM3Environment e) noexcept : env(e) {}
    EnvRAII(EnvRAII&& o) noexcept : env(o.env) { o.env = nullptr; }
    EnvRAII& operator=(EnvRAII&& o) noexcept { if (this != &o) { if (env) m3_FreeEnvironment(env); env = o.env; o.env = nullptr; } return *this; }
    EnvRAII(const EnvRAII&) = delete;
    EnvRAII& operator=(const EnvRAII&) = delete;
    ~EnvRAII() noexcept { if (env) { m3_FreeEnvironment(env); env = nullptr; } }
};

static std::mutex g_wasmMutex;
static std::unordered_map<std::string, std::unique_ptr<WasmModule>> g_modules;
static bool g_inited = false;
static EnvRAII g_env;

// Default resource limits (can be updated via SetDefaultResourceLimits)
static ResourceLimits g_defaultResourceLimits;
// Modules scheduled for deferred cleanup (e.g., timed-out modules that still have active calls)
static std::vector<std::unique_ptr<WasmModule>> g_shutdownModules;

bool WasmRuntime::Init() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    if (g_inited) return true;
    g_env = EnvRAII(m3_NewEnvironment());
    if (!g_env.env) {
        std::cerr << "WasmRuntime: failed to create wasm3 environment" << std::endl;
        return false;
    }
    g_inited = true;
    std::cout << "WasmRuntime: initialized" << std::endl;
    return true;
}

void WasmRuntime::Shutdown() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    // Destroy modules first (tokens will unregister and runtime/module will be freed in WasmModule destructors)
    std::cerr << "WasmRuntime::Shutdown: destroying " << g_modules.size() << " modules" << std::endl;
    for (auto &p : g_modules) {
        std::cerr << "WasmRuntime::Shutdown: module='" << p.first << "' ptr=" << p.second.get() << " runtime=" << p.second->runtime << " modulePtr=" << p.second->module << std::endl;
    }
    g_modules.clear();
    std::cerr << "WasmRuntime::Shutdown: modules cleared" << std::endl;
    if (g_env.env) { m3_FreeEnvironment(g_env.env); g_env.env = nullptr; }
    std::cerr << "WasmRuntime::Shutdown: environment freed" << std::endl;
    g_inited = false;
}

// Global host function registrations (available for subsequent module loads)
static std::mutex g_hostRegMutex;
struct GlobalHostRegistration { size_t id; std::string ns; std::string name; std::string sig; M3RawCall cb; };
static std::vector<GlobalHostRegistration> g_globalHostFunctions;
static size_t g_nextHostId = 1;

static void UnregisterGlobalHost(size_t id) {
    std::lock_guard<std::mutex> lk(g_hostRegMutex);
    auto it = std::remove_if(g_globalHostFunctions.begin(), g_globalHostFunctions.end(), [&](const GlobalHostRegistration &g){ return g.id == id; });
    if (it != g_globalHostFunctions.end()) g_globalHostFunctions.erase(it, g_globalHostFunctions.end());
}

// Forward declarations for host functions (M3 API raw-style signatures)
static const void* engine_create_body(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_destroy_body(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_apply_impulse(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_create_distance_joint(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);

// Host registration token lifecycle
WasmRuntime::HostBindingToken::HostBindingToken(HostBindingToken&& other) noexcept : id(other.id) { other.id = SIZE_MAX; }
WasmRuntime::HostBindingToken& WasmRuntime::HostBindingToken::operator=(HostBindingToken&& other) noexcept { if (this != &other) { if (id != SIZE_MAX) UnregisterGlobalHost(id); id = other.id; other.id = SIZE_MAX; } return *this; }
WasmRuntime::HostBindingToken::~HostBindingToken() noexcept { if (id != SIZE_MAX) UnregisterGlobalHost(id); }
bool WasmRuntime::HostBindingToken::valid() const noexcept { return id != SIZE_MAX; }

WasmRuntime::HostBindingToken WasmRuntime::RegisterHostFunction(const std::string& ns, const std::string& name, const std::string& sig, M3RawCall cb) {
    std::lock_guard<std::mutex> lk(g_hostRegMutex);
    size_t id = g_nextHostId++;
    g_globalHostFunctions.push_back({ id, ns, name, sig, cb });
    std::cout << "WasmRuntime: registered global host '" << ns << "'.'" << name << "' id=" << id << std::endl;
    return HostBindingToken(id);
}

void WasmRuntime::SetDefaultResourceLimits(const ResourceLimits& limits) {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    g_defaultResourceLimits = limits;
    std::cout << "WasmRuntime: default resource limits updated: memory=" << g_defaultResourceLimits.memory_limit_bytes << " bytes exec_ms=" << g_defaultResourceLimits.execution_time_ms << std::endl;
}

static const char kHostLinkFailed[] = "host link failed";
static M3Result LinkHostFunctions(WasmModule* wm) {
    if (!wm || !wm->module) return "invalid-module";
    std::cerr << "LinkHostFunctions: begin module='" << wm->name << "' modulePtr=" << wm->module << std::endl;
    HostBindings hb(wm->module);
    // Register any global host functions that were registered via WasmRuntime::RegisterHostFunction
    {
        std::lock_guard<std::mutex> lk(g_hostRegMutex);
        for (const auto &g : g_globalHostFunctions) {
            auto tok = hb.RegisterRaw(g.ns.c_str(), g.name.c_str(), g.sig.c_str(), g.cb);
            if (!tok.valid()) {
                std::cerr << "WasmRuntime: LinkHostFunctions: failed to bind global host '" << g.ns << "'.'" << g.name << "' to module '" << wm->name << "' (see earlier logs)" << std::endl;
                return kHostLinkFailed;
            }
            wm->hostTokens.push_back(std::move(tok));
        }
    }

    std::cerr << "LinkHostFunctions: done module='" << wm->name << "'" << std::endl;
    return m3Err_none;
}

// Return pointer to a module's timed_out flag (or nullptr). This is used by host trampolines
// to perform a fast, lock-free check without taking the global wasm mutex (avoids deadlocks).
std::atomic<bool>* WasmRuntime::GetModuleTimedOutPtr(IM3Module module) {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    for (auto &p : g_modules) {
        if (p.second && p.second->module == module) return &p.second->timed_out;
    }
    return nullptr;
}


// Host import implementations
m3ApiRawFunction(engine_create_body) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, mass_fixed);
    m3ApiGetArg(int32_t, x_fixed);
    m3ApiGetArg(int32_t, y_fixed);
    m3ApiGetArg(int32_t, sx_fixed);
    m3ApiGetArg(int32_t, sy_fixed);

    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    if (!ph) {
        m3ApiReturn(0);
    }
    float mass = mass_fixed / 1000.0f;
    float x = x_fixed / 1000.0f;
    float y = y_fixed / 1000.0f;
    float sx = sx_fixed / 1000.0f;
    float sy = sy_fixed / 1000.0f;
    auto h = ph->CreateBoxRigidBody(mass, x, y, 0.0f, sx, sy, 0.0f);
    m3ApiReturn((uint32_t)h);
}

m3ApiRawFunction(engine_destroy_body) {
    m3ApiGetArg(int32_t, h);
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    if (ph) ph->DestroyRigidBody((IPhysics::BodyHandle)h);
    m3ApiSuccess();
}

m3ApiRawFunction(engine_apply_impulse) {
    m3ApiGetArg(int32_t, h);
    m3ApiGetArg(int32_t, ix_fixed);
    m3ApiGetArg(int32_t, iy_fixed);
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    if (ph) ph->ApplyCentralImpulse((IPhysics::BodyHandle)h, ix_fixed / 1000.0f, iy_fixed / 1000.0f, 0.0f);
    m3ApiSuccess();
}

m3ApiRawFunction(engine_create_distance_joint) {
    m3ApiReturnType(uint32_t);
    m3ApiGetArg(int32_t, a);
    m3ApiGetArg(int32_t, b);
    m3ApiGetArg(int32_t, ax_fixed);
    m3ApiGetArg(int32_t, ay_fixed);
    m3ApiGetArg(int32_t, bx_fixed);
    m3ApiGetArg(int32_t, by_fixed);
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    if (!ph) { m3ApiReturn(0); }
    float ax = ax_fixed / 1000.0f;
    float ay = ay_fixed / 1000.0f;
    float bx = bx_fixed / 1000.0f;
    float by = by_fixed / 1000.0f;
    auto j = ph->CreateDistanceJoint((IPhysics::BodyHandle)a, (IPhysics::BodyHandle)b, ax, ay, bx, by);
    m3ApiReturn((uint32_t)j);
}

static bool LoadModuleBytes(const std::string& name, const std::vector<uint8_t>& bytes) {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    if (!g_inited) {
        if (!WasmRuntime::Init()) return false;
    }

    IM3Module module = nullptr;
    M3Result r = m3_ParseModule(g_env.env, &module, bytes.data(), bytes.size());
    if (r) {
        std::cerr << "WasmRuntime: m3_ParseModule failed for module '" << name << "': " << (r ? r : "(unknown)") << std::endl;
        return false;
    }

    // Create module container (so we can attach host tokens that must live as long as the module)
    auto wm = std::make_unique<WasmModule>();
    wm->name = name;
    wm->bytes = bytes;
    wm->module = module;

    // Create runtime and load the module into it BEFORE linking host imports. Wasm3 requires the module
    // to be associated with a runtime for m3_LinkRawFunction to succeed.
    // TODO: Enforce memory limits per-module (e.g., max memory pages) by examining module bytes and/or using
    // wasm3 APIs if available. See Docs/adr/0003-wasm-runtime.md and ticket GS-XXXX for follow-up.
    IM3Runtime runtime = m3_NewRuntime(g_env.env, 64*1024, NULL);
    if (!runtime) {
        std::cerr << "WasmRuntime: m3_NewRuntime failed for module '" << name << "'" << std::endl;
        // WasmModule destructor will free module
        return false;
    }

    r = m3_LoadModule(runtime, module);
    if (r) {
        std::cerr << "WasmRuntime: m3_LoadModule failed for module '" << name << "': " << (r ? r : "(unknown)") << std::endl;
        m3_FreeRuntime(runtime);
        // WasmModule destructor will free module
        return false;
    }

    // Attach the runtime to the module container so LinkHostFunctions can see it
    wm->runtime = runtime;

    // Link host imports into the loaded module (tokens are stored on the WasmModule so they outlive this function)
    r = LinkHostFunctions(wm.get());
    if (r) {
        std::cerr << "WasmRuntime: LinkHostFunctions failed for module '" << name << "': " << (r ? r : "(unknown)") << std::endl;
        // Do not free runtime here; let WasmModule destructor handle cleanup in the correct order (module then runtime)
        return false;
    }

    g_modules[name] = std::move(wm);
    std::cout << "WasmRuntime: loaded module '" << name << "'" << std::endl;
    return true;
}

bool WasmRuntime::LoadModule(const std::filesystem::path& modulePath) {
    if (!std::filesystem::exists(modulePath) || !std::filesystem::is_regular_file(modulePath)) return false;
    std::ifstream ifs(modulePath, std::ios::binary);
    if (!ifs) return false;
    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::vector<uint8_t> bytes(s.begin(), s.end());
    std::string name = modulePath.filename().string();
    if (!LoadModuleBytes(name, bytes)) return false;
    // Call mod_init if exported
    if (!CallExported(name, "mod_init")) {
        // not an error; module may not export mod_init
    }
    return true;
}

bool WasmRuntime::LoadModuleFromBytes(const std::string& moduleName, const std::vector<uint8_t>& bytes) {
    if (!LoadModuleBytes(moduleName, bytes)) return false;
    // Call mod_init if exported
    if (!CallExported(moduleName, "mod_init")) {
        // not an error; module may not export mod_init
    }
    return true;
}

bool WasmRuntime::CallExported(const std::string& moduleName, const std::string& funcName, const std::vector<std::string>& args) {
    // Call with the default execution time limit
    return CallExportedWithTimeout(moduleName, funcName, args, g_defaultResourceLimits.execution_time_ms);
}

bool WasmRuntime::CallExportedWithTimeout(const std::string& moduleName, const std::string& funcName, const std::vector<std::string>& args, uint32_t timeoutMs) {
    // Locate module and mark as active (increment in-flight counter)
    WasmModule* modulePtr = nullptr;
    {
        std::lock_guard<std::mutex> lk(g_wasmMutex);
        auto it = g_modules.find(moduleName);
        if (it == g_modules.end()) return false;
        modulePtr = it->second.get();
        if (modulePtr->timed_out.load()) {
            std::cerr << "WasmRuntime: rejecting call to timed-out module '" << moduleName << "' func '" << funcName << "'" << std::endl;
            return false;
        }
        modulePtr->active_calls.fetch_add(1);
    }

    // Run the call asynchronously so we can enforce a timeout. Capture the raw module pointer so the worker
    // can still decrement active_calls even if the module is removed from the map.
    auto fut = std::async(std::launch::async, [moduleName, funcName, args, modulePtr]() -> bool {
        WasmModule* m = modulePtr;
        if (!m) return false;
        if (m->timed_out.load()) return false;

        bool call_ok = false;
        // Perform the actual wasm call under the global wasm mutex so runtimes aren't freed concurrently
        {
            std::lock_guard<std::mutex> lk(g_wasmMutex);
            IM3Function f = nullptr;
            M3Result r = m3_FindFunction(&f, m->runtime, funcName.c_str());
            if (r) {
                std::cerr << "WasmRuntime: m3_FindFunction failed for module '" << moduleName << "' func '" << funcName << "': " << (r ? r : "(unknown)") << std::endl;
                call_ok = false;
            } else {
                std::vector<const char*> argv;
                argv.reserve(args.size());
                for (auto &s : args) argv.push_back(s.c_str());
                r = m3_CallArgv(f, static_cast<uint32_t>(argv.size()), argv.empty() ? nullptr : argv.data());
                if (r) {
                    std::cerr << "WasmRuntime: m3_CallArgv failed for module '" << moduleName << "' func '" << funcName << "': " << (r ? r : "(unknown)") << std::endl;
                    call_ok = false;
                } else {
                    call_ok = true;
                }
            }
        }

        // Decrement active calls and schedule cleanup if the module is timed out and no active calls left
        int prev = m->active_calls.fetch_sub(1);
        if (m->timed_out.load() && prev == 1) {
            std::lock_guard<std::mutex> lk(g_wasmMutex);
            auto it = g_modules.find(moduleName);
            if (it != g_modules.end() && it->second.get() == m) {
                std::cerr << "WasmRuntime: scheduling cleanup for module '" << moduleName << "' after timeout" << std::endl;
                g_shutdownModules.push_back(std::move(it->second));
                g_modules.erase(it);
            }
        }

        return call_ok;
    });

    // Wait for result with timeout
    auto status = fut.wait_for(std::chrono::milliseconds(timeoutMs));
    if (status == std::future_status::ready) {
        bool res = fut.get();
        return res;
    } else {
        // Timeout: mark module as timed out and log. Do not free resources synchronously while the module
        // might still be executing. The worker will decrement active call counters and schedule cleanup when safe.
        {
            // Set atomically without holding g_wasmMutex to avoid deadlocks; the cleanup will be scheduled by the worker thread
            std::lock_guard<std::mutex> lk(g_wasmMutex);
            auto it = g_modules.find(moduleName);
            if (it != g_modules.end()) it->second->timed_out.store(true);
        }
        std::cerr << "WasmRuntime: module '" << moduleName << "' func '" << funcName << "' timed out after " << timeoutMs << "ms" << std::endl;
        return false;
    }
}

std::vector<std::string> WasmRuntime::LoadedModules() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    std::vector<std::string> out;
    for (auto &p : g_modules) out.push_back(p.first);
    return out;
}

void WasmRuntime::RegisterPhysicsCallbacks(std::shared_ptr<IPhysics> phys) {
    if (!phys) return;
    // Setup a callback that will forward to any loaded module's 'on_contact_begin' / 'on_contact_end' exports
    phys->SetContactCallbacks(
            [](IPhysics::BodyHandle a, IPhysics::BodyHandle b) {
            std::lock_guard<std::mutex> lk(g_wasmMutex);
            for (auto &p : g_modules) {
                if (p.second->timed_out.load()) continue; // skip modules that are quarantined
                IM3Function f = nullptr;
                M3Result r = m3_FindFunction(&f, p.second->runtime, "on_contact_begin");
                if (!r) {
                    // call with arguments as strings
                    std::string sa = std::to_string(static_cast<long long>(a));
                    std::string sb = std::to_string(static_cast<long long>(b));
                        const char* argv[2] = { sa.c_str(), sb.c_str() };
                        M3Result cr = m3_CallArgv(f, 2, argv);
                        if (cr) std::cerr << "WasmRuntime: on_contact_begin call failed in module '" << p.second->name << "': " << (cr ? cr : "(unknown)") << std::endl;
                }
            }
        },
            [](IPhysics::BodyHandle a, IPhysics::BodyHandle b) {
            std::lock_guard<std::mutex> lk(g_wasmMutex);
            for (auto &p : g_modules) {
                if (p.second->timed_out.load()) continue; // skip modules that are quarantined
                IM3Function f = nullptr;
                M3Result r = m3_FindFunction(&f, p.second->runtime, "on_contact_end");
                if (!r) {
                    std::string sa = std::to_string(static_cast<long long>(a));
                    std::string sb = std::to_string(static_cast<long long>(b));
                        const char* argv[2] = { sa.c_str(), sb.c_str() };
                        M3Result cr = m3_CallArgv(f, 2, argv);
                        if (cr) std::cerr << "WasmRuntime: on_contact_end call failed in module '" << p.second->name << "': " << (cr ? cr : "(unknown)") << std::endl;
                }
            }
        }
    );
}

#else // HAVE_WASM3

bool WasmRuntime::Init() { std::cout << "WasmRuntime: wasm3 not available; runtime disabled" << std::endl; return false; }
void WasmRuntime::Shutdown() {}
bool WasmRuntime::LoadModule(const std::filesystem::path& /*modulePath*/) { return false; }
// Load from bytes: no-op when wasm3 not available
bool WasmRuntime::LoadModuleFromBytes(const std::string& /*moduleName*/, const std::vector<uint8_t>& /*bytes*/) { return false; }
bool WasmRuntime::CallExported(const std::string& /*moduleName*/, const std::string& /*funcName*/, const std::vector<std::string>& /*args*/) { return false; }
bool WasmRuntime::CallExportedWithTimeout(const std::string& /*moduleName*/, const std::string& /*funcName*/, const std::vector<std::string>& /*args*/, uint32_t /*timeoutMs*/) { return false; }
void WasmRuntime::SetDefaultResourceLimits(const ResourceLimits& /*limits*/) { }
void WasmRuntime::RegisterPhysicsCallbacks(std::shared_ptr<IPhysics> /*phys*/) { }
std::vector<std::string> WasmRuntime::LoadedModules() { return {}; }

#endif // HAVE_WASM3

} // namespace Genesis::Engine
