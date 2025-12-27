#include "engine/WasmRuntime.h"
#include "engine/SubsystemRegistry.h"
#include "engine/Engine.h"
#include <iostream>
#include <fstream>
#include <iterator>
#include <unordered_map>
#include <mutex>



#include "wasm3.h"
#include "m3_env.h"


namespace Genesis::Engine {

// Host implementations remain in the wasm-enabled section below

#ifdef HAVE_WASM3
#include "engine/WasmHostBindings.h"
#include <algorithm>

struct WasmModule {
    std::string name;
    std::vector<uint8_t> bytes;
    IM3Module module = nullptr;
    IM3Runtime runtime = nullptr;
    std::vector<HostBindings::Token> hostTokens;

    // Ensure resources are freed when module is destroyed; noexcept to avoid terminating during stack unwinding
    ~WasmModule() noexcept {
        if (runtime) { m3_FreeRuntime(runtime); runtime = nullptr; }
        if (module) { m3_FreeModule(module); module = nullptr; }
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
    g_modules.clear();
    if (g_env.env) { m3_FreeEnvironment(g_env.env); g_env.env = nullptr; }
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

static const char kHostLinkFailed[] = "host link failed";
static M3Result LinkHostFunctions(WasmModule* wm) {
    if (!wm || !wm->module) return "invalid-module";
    HostBindings hb(wm->module);
    // Register engine-level host functions
    auto t0 = hb.RegisterRaw("env", "engine_create_body", "i(iiiii)", engine_create_body);
    if (!t0.valid()) { std::cerr << "WasmRuntime: LinkHostFunctions failed to register 'engine_create_body' for module '" << wm->name << "'" << std::endl; return kHostLinkFailed; }
    wm->hostTokens.push_back(std::move(t0));

    auto t1 = hb.RegisterRaw("env", "engine_destroy_body", "v(i)", engine_destroy_body);
    if (!t1.valid()) { std::cerr << "WasmRuntime: LinkHostFunctions failed to register 'engine_destroy_body' for module '" << wm->name << "'" << std::endl; return kHostLinkFailed; }
    wm->hostTokens.push_back(std::move(t1));

    auto t2 = hb.RegisterRaw("env", "engine_apply_impulse", "v(iii)", engine_apply_impulse);
    if (!t2.valid()) { std::cerr << "WasmRuntime: LinkHostFunctions failed to register 'engine_apply_impulse' for module '" << wm->name << "'" << std::endl; return kHostLinkFailed; }
    wm->hostTokens.push_back(std::move(t2));

    auto t3 = hb.RegisterRaw("env", "engine_create_distance_joint", "i(iiiiii)", engine_create_distance_joint);
    if (!t3.valid()) { std::cerr << "WasmRuntime: LinkHostFunctions failed to register 'engine_create_distance_joint' for module '" << wm->name << "'" << std::endl; return kHostLinkFailed; }
    wm->hostTokens.push_back(std::move(t3));

    // Register any global host functions that were registered via WasmRuntime::RegisterHostFunction
    {
        std::lock_guard<std::mutex> lk(g_hostRegMutex);
        for (const auto &g : g_globalHostFunctions) {
            auto tok = hb.RegisterRaw(g.ns.c_str(), g.name.c_str(), g.sig.c_str(), g.cb);
            if (!tok.valid()) {
                std::cerr << "WasmRuntime: LinkHostFunctions failed to register global host '" << g.ns << "'.'" << g.name << "' for module '" << wm->name << "'" << std::endl;
                return kHostLinkFailed;
            }
            wm->hostTokens.push_back(std::move(tok));
        }
    }

    return m3Err_none;
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

    // Link host imports into the parsed module (tokens are stored on the WasmModule so they outlive this function)
    r = LinkHostFunctions(wm.get());
    if (r) {
        std::cerr << "WasmRuntime: LinkHostFunctions failed for module '" << name << "': " << (r ? r : "(unknown)") << std::endl;
        // WasmModule destructor will free module
        return false;
    }

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

    wm->runtime = runtime;
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
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    auto it = g_modules.find(moduleName);
    if (it == g_modules.end()) return false;
    auto& m = it->second;
    IM3Function f = nullptr;
    M3Result r = m3_FindFunction(&f, m->runtime, funcName.c_str());
    if (r) {
        std::cerr << "WasmRuntime: m3_FindFunction failed for module '" << moduleName << "' func '" << funcName << "': " << (r ? r : "(unknown)") << std::endl;
        return false;
    }
    // m3_CallArgv takes an array of C strings
    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (auto &s : args) argv.push_back(s.c_str());
    r = m3_CallArgv(f, static_cast<uint32_t>(argv.size()), argv.empty() ? nullptr : argv.data());
    if (r) {
        std::cerr << "WasmRuntime: m3_CallArgv failed for module '" << moduleName << "' func '" << funcName << "': " << (r ? r : "(unknown)") << std::endl;
        return false;
    }
    return true;
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
void WasmRuntime::RegisterPhysicsCallbacks(std::shared_ptr<IPhysics> /*phys*/) { }
std::vector<std::string> WasmRuntime::LoadedModules() { return {}; }

#endif // HAVE_WASM3

} // namespace Genesis::Engine
