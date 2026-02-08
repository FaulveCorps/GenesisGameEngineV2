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
#include <filesystem>
#include <iomanip>
#include <sstream>

#include "wasm3.h"
#include "m3_env.h"
#ifdef _WIN32
#include <windows.h>
#include <dbghelp.h>
#pragma comment(lib, "Dbghelp.lib")
#endif

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
    // Per-module resource limits (copied from runtime defaults at load time)
    ResourceLimits resourceLimits;


    // Ensure resources are freed when module is destroyed; noexcept to avoid terminating during stack unwinding
    ~WasmModule() noexcept {
        // Debug: trace destruction sequence
        std::cerr << "WasmModule::~WasmModule: destroying module='" << name << "' modulePtr=" << module << " runtime=" << runtime << std::endl;
        // Destroy host registration tokens first so they can unregister while module/runtime are still valid
        std::cerr << "WasmModule::~WasmModule: clearing " << hostTokens.size() << " tokens" << std::endl;
        hostTokens.clear();
        std::cerr << "WasmModule::~WasmModule: tokens cleared" << std::endl;
        // Save the module pointer so we can perform callback cleanup *after* the
        // module/runtime are freed. This ensures userdata (ImportUserdata objects)
        // remain valid until wasm3 has finished any internal cleanup that may touch
        // the userdata pointers.
        IM3Module saved_module = module;

        if (runtime) {
            std::cerr << "WasmModule::~WasmModule: freeing runtime (will also free loaded module)=" << runtime << std::endl;
            m3_FreeRuntime(runtime);
            runtime = nullptr;
            module = nullptr; // runtime freed associated module
            std::cerr << "WasmModule::~WasmModule: runtime freed; module pointer cleared" << std::endl;
            // Now it's safe to remove callbacks associated with the module
            if (saved_module) { HostBindings::CleanupModuleCallbacks(saved_module); }
        } else if (saved_module) {
            std::cerr << "WasmModule::~WasmModule: freeing modulePtr (not attached to runtime)=" << saved_module << std::endl;
            m3_FreeModule(saved_module);
            module = nullptr;
            std::cerr << "WasmModule::~WasmModule: module freed" << std::endl;
            // Now it's safe to remove callbacks associated with the module
            HostBindings::CleanupModuleCallbacks(saved_module);
        }

        // Process any deferred unregistrations now that module/runtime resources have been freed
        HostBindings::ProcessDeferredUnregistrations();
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

static std::mutex& g_wasmMutex = *new std::mutex;
static std::unordered_map<std::string, std::unique_ptr<WasmModule>>& g_modules = *new std::unordered_map<std::string, std::unique_ptr<WasmModule>>;
static bool g_inited = false;
static EnvRAII& g_env = *new EnvRAII;

// Default resource limits (can be updated via SetDefaultResourceLimits)
static ResourceLimits g_defaultResourceLimits;
// Modules scheduled for deferred cleanup (e.g., timed-out modules that still have active calls)
static std::vector<std::unique_ptr<WasmModule>>& g_shutdownModules = *new std::vector<std::unique_ptr<WasmModule>>;

static bool HasActiveCalls() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    for (auto &p : g_modules) {
        if (p.second && p.second->active_calls.load() > 0) return true;
    }
    for (auto &p : g_shutdownModules) {
        if (p && p->active_calls.load() > 0) return true;
    }
    return false;
}

static void WaitForActiveCalls(std::chrono::milliseconds maxWait) {
    auto start = std::chrono::steady_clock::now();
    while (HasActiveCalls()) {
        if (maxWait.count() > 0) {
            auto elapsed = std::chrono::steady_clock::now() - start;
            if (elapsed >= maxWait) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
}

// Forward declarations for host functions (M3 API raw-style signatures)
static const void* engine_create_body(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_destroy_body(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_apply_impulse(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_create_distance_joint(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_log(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);
static const void* engine_get_time(IM3Runtime runtime, IM3ImportContext _ctx, uint64_t* _sp, void* _mem);

bool WasmRuntime::Init() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    if (g_inited) return true;
    g_env = EnvRAII(m3_NewEnvironment());
    if (!g_env.env) {
        std::cerr << "WasmRuntime: failed to create wasm3 environment" << std::endl;
        return false;
    }
    g_inited = true;
    
    // Register default host functions
    RegisterHostFunction("env", "engine_create_body", "i(iiiii)", engine_create_body);
    RegisterHostFunction("env", "engine_destroy_body", "v(i)", engine_destroy_body);
    RegisterHostFunction("env", "engine_apply_impulse", "v(iii)", engine_apply_impulse);
    RegisterHostFunction("env", "engine_create_distance_joint", "i(iiiiii)", engine_create_distance_joint);
    RegisterHostFunction("env", "engine_log", "v(ii)", engine_log);
    RegisterHostFunction("env", "engine_get_time", "f()", engine_get_time);

    std::cout << "WasmRuntime: initialized" << std::endl;
    return true;
}

void WasmRuntime::Shutdown() {
    // Wait for any in-flight async calls to finish so we don't free runtimes out from under them.
    WaitForActiveCalls(std::chrono::milliseconds(5000));

    std::lock_guard<std::mutex> lk(g_wasmMutex);
    // Destroy modules first (tokens will unregister and runtime/module will be freed in WasmModule destructors)
    std::cerr << "WasmRuntime::Shutdown: destroying " << g_modules.size() << " modules" << std::endl;
    for (auto &p : g_modules) {
        std::cerr << "WasmRuntime::Shutdown: module='" << p.first << "' ptr=" << p.second.get() << " runtime=" << p.second->runtime << " modulePtr=" << p.second->module << std::endl;
    }
    g_modules.clear();
    std::cerr << "WasmRuntime::Shutdown: modules cleared" << std::endl;

    // Also clear any modules that were scheduled for deferred shutdown (e.g. timed out modules)
    std::cerr << "WasmRuntime::Shutdown: destroying " << g_shutdownModules.size() << " deferred modules" << std::endl;
    g_shutdownModules.clear();
    std::cerr << "WasmRuntime::Shutdown: deferred modules cleared" << std::endl;

    // Process any deferred unregistrations that were queued by Token destructors during teardown
    HostBindings::ProcessDeferredUnregistrations();

    if (g_env.env) { m3_FreeEnvironment(g_env.env); g_env.env = nullptr; }
    std::cerr << "WasmRuntime::Shutdown: environment freed" << std::endl;
    g_inited = false;
}

// Global host function registrations (available for subsequent module loads)
static std::mutex& g_hostRegMutex = *new std::mutex;
struct GlobalHostRegistration { size_t id; std::string ns; std::string name; std::string sig; M3RawCall cb; };
static std::vector<GlobalHostRegistration>& g_globalHostFunctions = *new std::vector<GlobalHostRegistration>;
static size_t g_nextHostId = 1;

static void UnregisterGlobalHost(size_t id) {
    std::lock_guard<std::mutex> lk(g_hostRegMutex);
    auto it = std::remove_if(g_globalHostFunctions.begin(), g_globalHostFunctions.end(), [&](const GlobalHostRegistration &g){ return g.id == id; });
    if (it != g_globalHostFunctions.end()) g_globalHostFunctions.erase(it, g_globalHostFunctions.end());
}

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
ResourceLimits WasmRuntime::GetDefaultResourceLimits() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    return g_defaultResourceLimits;
}
static const char kHostLinkFailed[] = "host link failed";
static M3Result LinkHostFunctions(WasmModule* wm) {
    if (!wm || !wm->module) return "invalid-module";
    std::cerr << "LinkHostFunctions: begin module='" << wm->name << "' modulePtr=" << wm->module << std::endl;
    HostBindings hb(wm->module);
    // Configure HostBindings with the module's execution time limit (best-effort)
    hb.set_execution_timeout_ms(static_cast<uint32_t>(wm->resourceLimits.execution_time_ms));
    // Copy global registrations under lock then link them without holding the global lock to avoid lock-order deadlocks
    std::vector<GlobalHostRegistration> regsCopy;
    {
        std::lock_guard<std::mutex> lk(g_hostRegMutex);
        regsCopy = g_globalHostFunctions;
    }
    for (const auto &g : regsCopy) {
        try {
            std::cerr << "LinkHostFunctions: linking host '" << g.ns << "'.'" << g.name << "' (thread " << std::this_thread::get_id() << ")" << std::endl;
            auto tok = hb.RegisterRaw(g.ns.c_str(), g.name.c_str(), g.sig.c_str(), g.cb, &wm->timed_out);
            if (!tok.valid()) {
                std::cerr << "WasmRuntime: LinkHostFunctions: failed to bind global host '" << g.ns << "'.'" << g.name << "' to module '" << wm->name << "' (see earlier logs)" << std::endl;
                return kHostLinkFailed;
            }
            wm->hostTokens.push_back(std::move(tok));
            std::cerr << "LinkHostFunctions: linked host '" << g.ns << "'.'" << g.name << "' (thread " << std::this_thread::get_id() << ")" << std::endl;
        } catch (const std::system_error &se) {
            std::cerr << "LinkHostFunctions: system_error while linking '" << g.ns << "'.'" << g.name << "': " << se.what() << " code=" << se.code().value() << std::endl;
#ifdef _WIN32
            void* addrs[64];
            USHORT frames = CaptureStackBackTrace(0, 64, addrs, nullptr);
            std::cerr << "Stack frames captured: " << frames << std::endl;
            HANDLE hProc = GetCurrentProcess();
            SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
            if (SymInitialize(hProc, NULL, TRUE)) {
                for (USHORT i = 0; i < frames; ++i) {
                    DWORD64 addr = (DWORD64)(addrs[i]);
                    char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                    PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
                    pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                    pSymbol->MaxNameLen = MAX_SYM_NAME;
                    DWORD64 displacement = 0;
                    if (SymFromAddr(hProc, addr, &displacement, pSymbol)) {
                        std::cerr << std::hex << pSymbol->Address << " " << pSymbol->Name << std::dec << " +0x" << displacement << std::endl;
                    } else {
                        std::cerr << std::hex << addr << std::dec << std::endl;
                    }
                }
            } else {
                for (USHORT i=0;i<frames;i++) std::cerr << addrs[i] << std::endl;
            }
#endif
            return kHostLinkFailed;
        } catch (const std::exception &ex) {
            std::cerr << "LinkHostFunctions: exception while linking '" << g.ns << "'.'" << g.name << "': " << ex.what() << std::endl;
            return kHostLinkFailed;
        } catch (...) {
            std::cerr << "LinkHostFunctions: unknown exception while linking '" << g.ns << "'.'" << g.name << "'" << std::endl;
            return kHostLinkFailed;
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

// Return configured per-module memory limit (bytes). Falls back to runtime default when module not found.
std::size_t WasmRuntime::GetModuleMemoryLimitBytes(IM3Module module) {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    for (auto &p : g_modules) {
        if (p.second && p.second->module == module) return p.second->resourceLimits.memory_limit_bytes;
    }
    return g_defaultResourceLimits.memory_limit_bytes;
}

// Return configured per-module execution timeout (milliseconds). Falls back to runtime default when module not found.
uint32_t WasmRuntime::GetModuleExecutionTimeoutMs(IM3Module module) {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    for (auto &p : g_modules) {
        if (p.second && p.second->module == module) return p.second->resourceLimits.execution_time_ms;
    }
    return g_defaultResourceLimits.execution_time_ms;
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

m3ApiRawFunction(engine_log) {
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, len);
    
    uint32_t memSz = 0;
    uint8_t* mem = m3_GetMemory(runtime, &memSz, 0);
    if (!mem) m3ApiTrap("memory access failed");
    
    if (ptr < 0 || len < 0 || (uint32_t)(ptr + len) > memSz) m3ApiTrap("bounds check failed");
    
    std::string msg((char*)mem + ptr, len);
    std::cout << "[WASM] " << msg << std::endl;
    
    m3ApiSuccess();
}

m3ApiRawFunction(engine_get_time) {
    m3ApiReturnType(float);
    static auto start = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float t = std::chrono::duration<float>(now - start).count();
    m3ApiReturn(t);
}

static bool LoadModuleBytes(const std::string& name, const std::vector<uint8_t>& bytes) {
    try {
        std::cerr << "WasmRuntime::LoadModuleBytes: ensuring runtime initialized" << std::endl;
        if (!g_inited) {
            if (!WasmRuntime::Init()) return false;
        }
        std::cerr << "WasmRuntime::LoadModuleBytes: attempting to acquire g_wasmMutex" << std::endl;
        std::lock_guard<std::mutex> lk(g_wasmMutex);
        std::cerr << "WasmRuntime::LoadModuleBytes: acquired g_wasmMutex" << std::endl;

        IM3Module module = nullptr;
        std::cerr << "WasmRuntime::LoadModuleBytes: about to call m3_ParseModule" << std::endl;
        M3Result r = m3_ParseModule(g_env.env, &module, bytes.data(), bytes.size());
        std::cerr << "WasmRuntime::LoadModuleBytes: m3_ParseModule returned r=" << (r ? r : "(none)") << " module=" << (void*)module << std::endl;
        if (r) {
            std::cerr << "WasmRuntime: m3_ParseModule failed for module '" << name << "': " << (r ? r : "(unknown)") << std::endl;
            return false;
        }
        std::cerr << "WasmRuntime::LoadModuleBytes: about to create WasmModule container (post-parse)" << std::endl;

        // Create module container (so we can attach host tokens that must live as long as the module)
        std::cerr << "WasmRuntime::LoadModuleBytes: about to create WasmModule container" << std::endl;
        auto wm = std::make_unique<WasmModule>();
        std::cerr << "WasmRuntime::LoadModuleBytes: created WasmModule container" << std::endl;
        wm->name = name;
        wm->bytes = bytes;
        wm->module = module;
        // Initialize this module's resource limits from the current runtime defaults
        wm->resourceLimits = g_defaultResourceLimits;
        std::cout << "WasmRuntime::LoadModuleBytes: module resource limits set: memory=" << wm->resourceLimits.memory_limit_bytes << " bytes exec_ms=" << wm->resourceLimits.execution_time_ms << "ms" << std::endl;

        // Create runtime and load the module into it BEFORE linking host imports. Wasm3 requires the module
        // to be associated with a runtime for m3_LinkRawFunction to succeed.
        std::cerr << "WasmRuntime::LoadModuleBytes: about to create runtime" << std::endl;
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

        // Enforce per-module initial memory limits (if module declares memory)
        try {
            uint32_t memSz = 0;
            uint8_t* memPtr = nullptr;
            try { memPtr = m3_GetMemory(runtime, &memSz, 0); } catch(...) { memPtr = nullptr; memSz = 0; }
            if (memPtr && memSz > wm->resourceLimits.memory_limit_bytes) {
                std::cerr << "WasmRuntime: module '" << name << "' initial memory " << memSz << " exceeds limit " << wm->resourceLimits.memory_limit_bytes << " bytes" << std::endl;
                // Free the runtime (which also frees the loaded module in wasm3). Clear wm->module so the WasmModule
                // destructor does not attempt to free the module again (avoids double-free).
                m3_FreeRuntime(runtime);
                wm->module = nullptr;
                // WasmModule destructor will not free module now
                return false;
            }
        } catch (...) {
            std::cerr << "WasmRuntime: exception while checking initial memory for module '" << name << "'" << std::endl;
        }

        // Attach the runtime to the module container so LinkHostFunctions can see it
        wm->runtime = runtime;

        // Link host imports into the loaded module (tokens are stored on the WasmModule so they outlive this function)
        {
            std::cerr << "WasmRuntime::LoadModuleBytes: LinkHostFunctions start (thread " << std::this_thread::get_id() << ")" << std::endl;
            try {
                r = LinkHostFunctions(wm.get());
            } catch (const std::system_error &se) {
                std::cerr << "WasmRuntime::LoadModuleBytes: LinkHostFunctions threw system_error: " << se.what() << " code=" << se.code().value() << std::endl;
#ifdef _WIN32
                void* addrs[64];
                USHORT frames = CaptureStackBackTrace(0, 64, addrs, nullptr);
                std::cerr << "Stack frames captured: " << frames << std::endl;
                HANDLE hProc = GetCurrentProcess();
                SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
                if (SymInitialize(hProc, NULL, TRUE)) {
                    for (USHORT i = 0; i < frames; ++i) {
                        DWORD64 addr = (DWORD64)(addrs[i]);
                        char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                        PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
                        pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                        pSymbol->MaxNameLen = MAX_SYM_NAME;
                        DWORD64 displacement = 0;
                        if (SymFromAddr(hProc, addr, &displacement, pSymbol)) {
                            std::cerr << std::hex << pSymbol->Address << " " << pSymbol->Name << std::dec << " +0x" << displacement << std::endl;
                        } else {
                            std::cerr << std::hex << addr << std::dec << std::endl;
                        }
                    }
                } else {
                    for (USHORT i=0;i<frames;i++) std::cerr << addrs[i] << std::endl;
                }
#endif
                return false;
            } catch (const std::exception &ex) {
                std::cerr << "WasmRuntime::LoadModuleBytes: LinkHostFunctions threw exception: " << ex.what() << std::endl;
                return false;
            } catch (...) {
                std::cerr << "WasmRuntime::LoadModuleBytes: LinkHostFunctions threw unknown exception" << std::endl;
                return false;
            }
            std::cerr << "WasmRuntime::LoadModuleBytes: LinkHostFunctions end (thread " << std::this_thread::get_id() << ")" << std::endl;
        }
        if (r) {
            std::cerr << "WasmRuntime: LinkHostFunctions failed for module '" << name << "': " << (r ? r : "(unknown)") << std::endl;
            // Do not free runtime here; let WasmModule destructor handle cleanup in the correct order (module then runtime)
            return false;
        }

        g_modules[name] = std::move(wm);
        std::cout << "WasmRuntime: loaded module '" << name << "'" << std::endl;
        return true;
    } catch (const std::system_error &se) {
        std::cerr << "WasmRuntime::LoadModuleBytes: system_error: " << se.what() << " code=" << se.code().value() << std::endl;
#ifdef _WIN32
        void* addrs[64];
        USHORT frames = CaptureStackBackTrace(0, 64, addrs, nullptr);
        std::cerr << "Stack frames captured: " << frames << std::endl;
        HANDLE hProc = GetCurrentProcess();
        SymSetOptions(SYMOPT_DEFERRED_LOADS | SYMOPT_UNDNAME);
        if (SymInitialize(hProc, NULL, TRUE)) {
            for (USHORT i = 0; i < frames; ++i) {
                DWORD64 addr = (DWORD64)(addrs[i]);
                char buffer[sizeof(SYMBOL_INFO) + MAX_SYM_NAME * sizeof(TCHAR)];
                PSYMBOL_INFO pSymbol = (PSYMBOL_INFO)buffer;
                pSymbol->SizeOfStruct = sizeof(SYMBOL_INFO);
                pSymbol->MaxNameLen = MAX_SYM_NAME;
                DWORD64 displacement = 0;
                if (SymFromAddr(hProc, addr, &displacement, pSymbol)) {
                    std::cerr << std::hex << pSymbol->Address << " " << pSymbol->Name << std::dec << " +0x" << displacement << std::endl;
                } else {
                    std::cerr << std::hex << addr << std::dec << std::endl;
                }
            }
        } else {
            for (USHORT i=0;i<frames;i++) std::cerr << addrs[i] << std::endl;
        }
#endif
        return false;
    } catch (const std::exception &ex) {
        std::cerr << "WasmRuntime::LoadModuleBytes: exception: " << ex.what() << std::endl;
        return false;
    } catch (...) {
        std::cerr << "WasmRuntime::LoadModuleBytes: unknown exception" << std::endl;
        return false;
    }
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
    // Prefer a module-specific execution limit if configured, otherwise fall back to runtime default
    uint32_t timeoutMs = g_defaultResourceLimits.execution_time_ms;
    {
        std::lock_guard<std::mutex> lk(g_wasmMutex);
        auto it = g_modules.find(moduleName);
        if (it != g_modules.end()) timeoutMs = it->second->resourceLimits.execution_time_ms;
    }
    return CallExportedWithTimeout(moduleName, funcName, args, timeoutMs);
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
            std::cerr << "WasmRuntime: about to find function '" << funcName << "' in module '" << moduleName << "' (runtime=" << m->runtime << ")" << std::endl;
            M3Result r = m3_FindFunction(&f, m->runtime, funcName.c_str());
            std::cerr << "WasmRuntime: m3_FindFunction returned r=" << (r ? r : "(none)") << " f=" << (void*)f << std::endl;
            if (r) {
                std::cerr << "WasmRuntime: m3_FindFunction failed for module '" << moduleName << "' func '" << funcName << "': " << (r ? r : "(unknown)") << " (ptr=" << (void*)r << ")" << std::endl;
                call_ok = false;
            } else {
                std::vector<const char*> argv;
                argv.reserve(args.size());
                for (auto &s : args) argv.push_back(s.c_str());
                std::cerr << "WasmRuntime: about to call function f=" << (void*)f << " module='" << moduleName << "' func='" << funcName << "'" << std::endl;
#ifdef HAVE_WASM3
                // Print module memory info to ensure linear memory is valid
                try {
                    uint32_t memSz = 0;
                    uint8_t* memPtr = m3_GetMemory(m->runtime, &memSz, 0);
                    std::cerr << "WasmRuntime: module memory ptr=" << (void*)memPtr << " size=" << memSz << std::endl;
                } catch(...) {
                    std::cerr << "WasmRuntime: m3_GetMemory threw or unavailable" << std::endl;
                }
#endif
#ifdef _DEBUG
                HostBindings::DebugDumpCallbacksSnapshot("CallExported - before m3_CallArgv");
#endif
                r = m3_CallArgv(f, static_cast<uint32_t>(argv.size()), argv.empty() ? nullptr : argv.data());
                std::cerr << "WasmRuntime: m3_CallArgv returned r=" << (r ? r : "(none)") << " (ptr=" << (void*)r << ")" << std::endl;
                if (r) {
#ifdef _DEBUG
                    HostBindings::DebugDumpCallbacksSnapshot("CallExported - after m3_CallArgv failed");
#endif
                    std::cerr << "WasmRuntime: m3_CallArgv failed for module '" << moduleName << "' func '" << funcName << "': " << (r ? r : "(unknown)") << " (ptr=" << (void*)r << ")" << std::endl;
                    // Dump up to 256 bytes of the error string in hex to help debugging non-printable messages
                    const char* err = r;
                    std::cerr << "WasmRuntime: m3_CallArgv error bytes:";
                    for (int i=0; i<256 && err && err[i]; ++i) std::cerr << " " << std::hex << (int)(uint8_t)err[i];
                    std::cerr << std::dec << std::endl;

                    // Prepare variables we may use for deeper analysis (try to read err ptr and module memory)
#ifdef _WIN32
                    char membuf[256]; SIZE_T bytesRead = 0; bool err_mem_ok = false; DWORD rpmErr = 0;
                    uint8_t* memPtr2 = nullptr; uint32_t memSz2 = 0;
                    if (err) {
                        if (ReadProcessMemory(GetCurrentProcess(), (LPCVOID)err, membuf, sizeof(membuf), &bytesRead) && bytesRead > 0) {
                            err_mem_ok = true;
                            std::cerr << "WasmRuntime: m3_CallArgv raw memory at " << (void*)err << " bytes:";
                            for (SIZE_T i = 0; i < bytesRead && i < 64; ++i) std::cerr << " " << std::hex << (int)(uint8_t)membuf[i];
                            std::cerr << std::dec << std::endl;
                        } else {
                            rpmErr = GetLastError();
                            std::cerr << "WasmRuntime: ReadProcessMemory failed on m3_CallArgv err ptr, GetLastError=" << rpmErr << std::endl;
                        }
                    }
                    try {
                        memPtr2 = m3_GetMemory(m->runtime, &memSz2, 0);
                        if (memPtr2 && memSz2 > 0) {
                            std::cerr << "WasmRuntime: module mem[0..15]=";
                            for (uint32_t ii=0; ii<16 && ii<memSz2; ++ii) std::cerr << " " << std::hex << (int)memPtr2[ii];
                            std::cerr << std::dec << std::endl;
                        }
                    } catch(...) {
                        std::cerr << "WasmRuntime: m3_GetMemory threw or unavailable in error branch" << std::endl;
                    }
#endif
                    // Try to get richer error info from wasm3 runtime and write an on-disk callsite snapshot for offline analysis
                    try {
                        M3ErrorInfo ei{0};
                        m3_GetErrorInfo(m->runtime, &ei);
                        std::cerr << "WasmRuntime: m3_GetErrorInfo: result=" << (ei.result ? ei.result : "(null)") << " file=" << (ei.file ? ei.file : "(null)") << " line=" << ei.line << " message=" << (ei.message ? ei.message : "(null)") << std::endl;
                        if (ei.function) {
                            const char* fname = m3_GetFunctionName(ei.function);
                            std::cerr << "WasmRuntime: m3_GetErrorInfo: function=" << (fname ? fname : "(unknown)") << std::endl;
                        }
                        // Try to obtain a wasm-level backtrace
                        try {
                            IM3BacktraceInfo bt = m3_GetBacktrace(m->runtime);
                            if (bt && bt->frames) {
                                std::cerr << "WasmRuntime: m3 backtrace:";
                                for (IM3BacktraceFrame fr = bt->frames; fr; fr = fr->next) {
                                    const char* fname = fr->function ? m3_GetFunctionName(fr->function) : nullptr;
                                    std::cerr << "  func=" << (fname ? fname : "(unknown)") << " offset=" << fr->moduleOffset;
                                }
                                std::cerr << std::endl;
                            }
                        } catch (...) {
                            std::cerr << "WasmRuntime: m3_GetBacktrace threw or unavailable" << std::endl;
                        }

#ifdef _DEBUG
                        try {
                            // Ensure analysis dir exists
                            std::filesystem::path analysis_dir = std::filesystem::path("Tools") / "external" / "procdump" / "dumps" / "analysis";
                            std::error_code ec;
                            std::filesystem::create_directories(analysis_dir, ec);

                            SYSTEMTIME st; GetLocalTime(&st);
                            char tbuf[64]; sprintf_s(tbuf, "%04d%02d%02d_%02d%02d%02d", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
                            auto sanitize = [](const std::string &s) {
                                std::string out; out.reserve(s.size());
                                for (char c : s) {
                                    if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c=='.') out.push_back(c);
                                    else out.push_back('_');
                                }
                                return out;
                            };

                            std::string fname = std::string("callsite_") + tbuf + "_" + sanitize(moduleName) + "_" + sanitize(funcName) + ".txt";
                            std::filesystem::path outp = analysis_dir / fname;
                            std::ofstream ofs(outp);
                            if (ofs) {
                                ofs << "timestamp: " << tbuf << "\n";
                                ofs << "context: CallExported_m3_CallArgv_failed\n";
                                ofs << "module: " << moduleName << "\n";
                                ofs << "func: " << funcName << "\n";
                                ofs << "modulePtr: " << m->module << " runtime: " << m->runtime << "\n";
                                ofs << "function ptr: " << (void*)f << "\n";
                                ofs << "m3_CallArgv result ptr: " << (void*)r << "\n";
#ifdef _WIN32
                                if (err_mem_ok) {
                                    ofs << "err raw bytes (first " << bytesRead << " bytes): ";
                                    for (SIZE_T i = 0; i < bytesRead && i < 256; ++i) ofs << std::hex << (int)(uint8_t)membuf[i] << " ";
                                    ofs << std::dec << "\n";
                                } else {
                                    ofs << "err mem read failed, GetLastError=" << rpmErr << "\n";
                                }
                                if (memPtr2 && memSz2 > 0) {
                                    ofs << "module mem[0..15]: ";
                                    for (uint32_t ii=0; ii<16 && ii<memSz2; ++ii) ofs << std::hex << (int)memPtr2[ii] << " ";
                                    ofs << std::dec << "\n";
                                }
#endif
                                ofs << "m3_GetErrorInfo: result=" << (ei.result ? ei.result : "(null)") << " file=" << (ei.file ? ei.file : "(null)") << " line=" << ei.line << " message=" << (ei.message ? ei.message : "(null)") << "\n";
                                if (ei.function) ofs << "m3_error_function: " << (m3_GetFunctionName(ei.function) ? m3_GetFunctionName(ei.function) : "(unknown)") << "\n";
                                // Capture current registers for convenience
                                CONTEXT ctx; RtlCaptureContext(&ctx);
#ifdef _M_X64
                                ofs << "Registers: Rax=" << std::hex << ctx.Rax << " Rcx=" << ctx.Rcx << " Rdx=" << ctx.Rdx << " Rbx=" << ctx.Rbx << " Rsp=" << ctx.Rsp << " Rbp=" << ctx.Rbp << " Rsi=" << ctx.Rsi << " Rdi=" << ctx.Rdi << " Rip=" << ctx.Rip << std::dec << "\n";
#else
                                ofs << "Registers: Eip=" << std::hex << ctx.Eip << std::dec << "\n";
#endif
                                // callback counts
                                ofs << "callbacks_count: " << HostBindings::DebugGetCallbacksCount() << " deferred_count: " << HostBindings::DebugGetDeferredCount() << "\n";
                                ofs << "hostTokens.size: " << m->hostTokens.size() << "\n";
                                ofs << "END\n";
                                ofs.close();
                                std::cerr << "WasmRuntime: wrote callsite analysis " << outp << std::endl;

                                // Also write a safe callbacks snapshot to the main log to correlate behaviors
                                HostBindings::DebugDumpCallbacksSnapshot(std::string("CallExported - callsite snapshot: ") + moduleName + ":" + funcName);
                            } else {
                                std::cerr << "WasmRuntime: failed to open callsite analysis file " << outp << std::endl;
                            }
                        } catch (...) {
                            std::cerr << "WasmRuntime: exception while writing callsite analysis" << std::endl;
                        }
#endif
                        m3_ResetErrorInfo(m->runtime);
                    } catch (...) {
                        std::cerr << "WasmRuntime: m3_GetErrorInfo threw or unavailable" << std::endl;
                    }
#ifdef _WIN32
#ifdef _DEBUG
                    // Capture caller CONTEXT here so we preserve the registers at the
                    // moment m3_CallArgv failed (this is the most useful view for root-cause analysis)
                    CONTEXT dbg_ctx;
                    RtlCaptureContext(&dbg_ctx);
                    HostBindings::DebugWriteMiniDumpWithContext("CallExported_m3_CallArgv_failed", &dbg_ctx);
                    if (moduleName == "ping_mod" && funcName == "mod_init") {
                        DebugBreak();
                    }
#endif
#endif
                    call_ok = false;
                } else {
                    // After successful call, enforce per-module memory limit in case the module grew memory during execution
                    try {
                        uint32_t memSz2 = 0;
                        uint8_t* memPtr2 = nullptr;
                        try { memPtr2 = m3_GetMemory(m->runtime, &memSz2, 0); } catch(...) { memPtr2 = nullptr; memSz2 = 0; }
                        if (memPtr2) {
                            std::size_t memLimit = m->resourceLimits.memory_limit_bytes;
                            if (memSz2 > memLimit) {
                                std::cerr << "WasmRuntime: module '" << moduleName << "' exceeded memory limit during call (" << memSz2 << " > " << memLimit << "), quarantining" << std::endl;
                                if (!m->timed_out.load()) m->timed_out.store(true);
                                call_ok = false;
                            } else {
                                call_ok = true;
                            }
                        } else {
                            call_ok = true;
                        }
                    } catch(...) {
                        call_ok = true;
                    }
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
    std::cerr << "WasmRuntime: CallExportedWithTimeout: waiting up to " << timeoutMs << "ms for '" << moduleName << "'.'" << funcName << "'" << std::endl;
    auto waitStart = std::chrono::steady_clock::now();
    auto status = fut.wait_for(std::chrono::milliseconds(timeoutMs));
    auto waitEnd = std::chrono::steady_clock::now();
    auto waitDur = std::chrono::duration_cast<std::chrono::milliseconds>(waitEnd - waitStart).count();
    if (status == std::future_status::ready) {
        bool res = fut.get();
        std::cerr << "WasmRuntime: CallExportedWithTimeout: completed '" << moduleName << "'.'" << funcName << "' result=" << res << " wait_ms=" << waitDur << std::endl;
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
        std::cerr << "WasmRuntime: module '" << moduleName << "' func '" << funcName << "' timed out after " << timeoutMs << "ms (waited=" << waitDur << "ms)" << std::endl;
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
