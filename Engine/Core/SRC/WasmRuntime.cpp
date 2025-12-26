#include "engine/WasmRuntime.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>
#include <unordered_map>
#include <mutex>

#ifdef HAVE_WASM3
#include "wasm3.h"
#include "m3_env.h"
#endif

namespace Genesis::Engine {

#ifdef HAVE_WASM3
struct WasmModule {
    std::string name;
    std::vector<uint8_t> bytes;
    IM3Environment env = nullptr;
    IM3Runtime runtime = nullptr;
    IM3Module module = nullptr;
};

static std::mutex g_wasmMutex;
static std::unordered_map<std::string, std::unique_ptr<WasmModule>> g_modules;
static bool g_inited = false;
static IM3Environment g_env = nullptr;

bool WasmRuntime::Init() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    if (g_inited) return true;
    g_env = m3_NewEnvironment();
    if (!g_env) {
        std::cerr << "WasmRuntime: failed to create wasm3 environment" << std::endl;
        return false;
    }
    g_inited = true;
    std::cout << "WasmRuntime: initialized" << std::endl;
    return true;
}

void WasmRuntime::Shutdown() {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    for (auto &p : g_modules) {
        auto &m = p.second;
        if (m->runtime) { m3_FreeRuntime(m->runtime); m->runtime = nullptr; }
        if (m->module) { m3_FreeModule(m->module); m->module = nullptr; }
    }
    g_modules.clear();
    if (g_env) { m3_FreeEnvironment(g_env); g_env = nullptr; }
    g_inited = false;
}

static M3Result LinkHostFunctions(IM3Module module) {
    // Link host functions under module namespace 'env'
    // engine_create_body: i32 create_body(i32 mass_fixed, i32 x_fixed, i32 y_fixed, i32 sx_fixed, i32 sy_fixed)
    M3Result r = m3_LinkRawFunction(module, "env", "engine_create_body", "i(iiiii)", (void*)&engine_create_body);
    if (r) return r;
    // engine_destroy_body: void destroy_body(i32 handle)
    r = m3_LinkRawFunction(module, "env", "engine_destroy_body", "v(i)", (void*)&engine_destroy_body);
    if (r) return r;
    // engine_apply_impulse: void apply_impulse(i32 handle, i32 ix_fixed, i32 iy_fixed)
    r = m3_LinkRawFunction(module, "env", "engine_apply_impulse", "v(iii)", (void*)&engine_apply_impulse);
    if (r) return r;
    // engine_create_distance_joint: i32 create_distance_joint(i32 a, i32 b, i32 ax_fixed, i32 ay_fixed, i32 bx_fixed, i32 by_fixed)
    r = m3_LinkRawFunction(module, "env", "engine_create_distance_joint", "i(iiiiii)", (void*)&engine_create_distance_joint);
    return r;
}

// Host import implementations
m3ApiRawFunction(engine_create_body) {
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
    if (ph) ph->DestroyRigidBody((BodyHandle)h);
    m3ApiReturn;
}

m3ApiRawFunction(engine_apply_impulse) {
    m3ApiGetArg(int32_t, h);
    m3ApiGetArg(int32_t, ix_fixed);
    m3ApiGetArg(int32_t, iy_fixed);
    auto ph = Genesis::Engine::GetPhysicsSubsystem();
    if (ph) ph->ApplyCentralImpulse((BodyHandle)h, ix_fixed / 1000.0f, iy_fixed / 1000.0f, 0.0f);
    m3ApiReturn;
}

m3ApiRawFunction(engine_create_distance_joint) {
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
    auto j = ph->CreateDistanceJoint((BodyHandle)a, (BodyHandle)b, ax, ay, bx, by);
    m3ApiReturn((uint32_t)j);
}

static bool LoadModuleBytes(const std::string& name, const std::vector<uint8_t>& bytes) {
    std::lock_guard<std::mutex> lk(g_wasmMutex);
    if (!g_inited) {
        if (!WasmRuntime::Init()) return false;
    }

    IM3Module module = nullptr;
    M3Result r = m3_ParseModule(g_env, &module, bytes.data(), bytes.size());
    if (r) {
        std::cerr << "WasmRuntime: m3_ParseModule failed: " << r << std::endl;
        return false;
    }

    // Link host imports
    r = LinkHostFunctions(module);
    if (r) {
        std::cerr << "WasmRuntime: LinkHostFunctions failed: " << r << std::endl;
        m3_FreeModule(module);
        return false;
    }

    IM3Runtime runtime = m3_NewRuntime(g_env, 64*1024, NULL);
    if (!runtime) {
        std::cerr << "WasmRuntime: m3_NewRuntime failed" << std::endl;
        m3_FreeModule(module);
        return false;
    }

    r = m3_LoadModule(runtime, module);
    if (r) {
        std::cerr << "WasmRuntime: m3_LoadModule failed: " << r << std::endl;
        m3_FreeRuntime(runtime);
        m3_FreeModule(module);
        return false;
    }

    auto wm = std::make_unique<WasmModule>();
    wm->name = name;
    wm->bytes = bytes;
    wm->env = g_env;
    wm->runtime = runtime;
    wm->module = module;

    g_modules[name] = std::move(wm);
    std::cout << "WasmRuntime: loaded module '" << name << "'" << std::endl;
    return true;
}

bool WasmRuntime::LoadModule(const std::filesystem::path& modulePath) {
    if (!std::filesystem::exists(modulePath) || !std::filesystem::is_regular_file(modulePath)) return false;
    std::ifstream ifs(modulePath, std::ios::binary);
    if (!ifs) return false;
    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::string name = modulePath.filename().string();
    if (!LoadModuleBytes(name, bytes)) return false;
    // Call mod_init if exported
    if (!CallExported(name, "mod_init")) {
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
    if (r) return false;
    // m3_CallArgv takes an array of C strings
    std::vector<const char*> argv;
    argv.reserve(args.size());
    for (auto &s : args) argv.push_back(s.c_str());
    r = m3_CallArgv(f, static_cast<uint32_t>(argv.size()), argv.empty() ? nullptr : const_cast<char**>(argv.data()));
    if (r) {
        std::cerr << "WasmRuntime: call '" << funcName << "' failed: " << r << std::endl;
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
        [](BodyHandle a, BodyHandle b) {
            std::lock_guard<std::mutex> lk(g_wasmMutex);
            for (auto &p : g_modules) {
                IM3Function f = nullptr;
                M3Result r = m3_FindFunction(&f, p.second->runtime, "on_contact_begin");
                if (r == m3Err_none) {
                    // call with arguments as strings
                    std::string sa = std::to_string(static_cast<long long>(a));
                    std::string sb = std::to_string(static_cast<long long>(b));
                    const char* argv[2] = { sa.c_str(), sb.c_str() };
                    m3_CallArgv(f, 2, (char**)argv);
                }
            }
        },
        [](BodyHandle a, BodyHandle b) {
            std::lock_guard<std::mutex> lk(g_wasmMutex);
            for (auto &p : g_modules) {
                IM3Function f = nullptr;
                M3Result r = m3_FindFunction(&f, p.second->runtime, "on_contact_end");
                if (r == m3Err_none) {
                    std::string sa = std::to_string(static_cast<long long>(a));
                    std::string sb = std::to_string(static_cast<long long>(b));
                    const char* argv[2] = { sa.c_str(), sb.c_str() };
                    m3_CallArgv(f, 2, (char**)argv);
                }
            }
        }
    );
}

#else // HAVE_WASM3

bool WasmRuntime::Init() { std::cout << "WasmRuntime: wasm3 not available; runtime disabled" << std::endl; return false; }
void WasmRuntime::Shutdown() {}
bool WasmRuntime::LoadModule(const std::filesystem::path& /*modulePath*/) { return false; }
bool WasmRuntime::CallExported(const std::string& /*moduleName*/, const std::string& /*funcName*/, const std::vector<std::string>& /*args*/) { return false; }
void WasmRuntime::RegisterPhysicsCallbacks(std::shared_ptr<IPhysics> /*phys*/) { }
std::vector<std::string> WasmRuntime::LoadedModules() { return {}; }

#endif // HAVE_WASM3

} // namespace Genesis::Engine
