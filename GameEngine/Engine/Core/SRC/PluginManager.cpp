#include "engine/PluginManager.h"
#include "engine/PluginAPI.h"
#include "engine/SubsystemRegistry.h"
#include "engine/ISubsystem.h"

#include <iostream>
#include <vector>

// Exposed to plugins: registration callback. Plugins call this (C linkage) to register
// factories for subsystem backends:
//   void Engine_RegisterFactory(const char* subsystemType, const char* name, void* (*factory)())
extern "C" void Engine_RegisterFactory(const char* subsystemType, const char* name, void* (*factory)()) {
    using namespace Genesis::Engine;
    SubsystemRegistry::Instance().RegisterFactory(subsystemType, name, [factory]() {
        ISubsystem* p = reinterpret_cast<ISubsystem*>(factory());
        return std::unique_ptr<ISubsystem>(p);
    });
} 

#ifdef _WIN32
#include <Windows.h>
#else
#include <dlfcn.h>
#endif

namespace Genesis::Engine {

struct PluginManager::Impl {
#ifdef _WIN32
    std::vector<HMODULE> handles;
#else
    std::vector<void*> handles;
#endif
};

PluginManager::~PluginManager() {
    UnloadAll();
    delete m_impl;
}

bool PluginManager::LoadPlugin(const std::string& path) {
    if (!m_impl) m_impl = new Impl();

#ifdef _WIN32
    HMODULE h = LoadLibraryA(path.c_str());
    if (!h) {
        std::cerr << "Failed to load plugin: " << path << " (Error: " << GetLastError() << ")\n";
        return false;
    }
    auto init = (Plugin_Init_Fn)GetProcAddress(h, "Plugin_Init");
    auto name = (Plugin_Name_Fn)GetProcAddress(h, "Plugin_Name");
    auto shutdown = (Plugin_Shutdown_Fn)GetProcAddress(h, "Plugin_Shutdown");
    if (!init || !shutdown || !name) {
        std::cerr << "Plugin missing required entry points: " << path << std::endl;
        FreeLibrary(h);
        return false;
    }
    if (!init()) {
        std::cerr << "Plugin init failed: " << path << std::endl;
        FreeLibrary(h);
        return false;
    }
    std::cout << "Loaded plugin: " << name() << std::endl;

    // Optional: call plugin's registration callback so it can register additional subsystems
    auto reg = (Plugin_RegisterSubsystems_Fn)GetProcAddress(h, "Plugin_RegisterSubsystems");
    if (reg) {
        reg(&Engine_RegisterFactory);
    }

    m_impl->handles.push_back(h);
    return true;
#else
    void* h = dlopen(path.c_str(), RTLD_NOW);
    if (!h) {
        std::cerr << "Failed to load plugin: " << path << " (" << dlerror() << ")\n";
        return false;
    }
    auto init = (Plugin_Init_Fn)dlsym(h, "Plugin_Init");
    auto name = (Plugin_Name_Fn)dlsym(h, "Plugin_Name");
    auto shutdown = (Plugin_Shutdown_Fn)dlsym(h, "Plugin_Shutdown");
    if (!init || !shutdown || !name) {
        std::cerr << "Plugin missing required entry points: " << path << std::endl;
        dlclose(h);
        return false;
    }
    if (!init()) {
        std::cerr << "Plugin init failed: " << path << std::endl;
        dlclose(h);
        return false;
    }
    std::cout << "Loaded plugin: " << name() << std::endl;

    // Optional: call plugin's registration callback so it can register additional subsystems
    auto reg = (Plugin_RegisterSubsystems_Fn)dlsym(h, "Plugin_RegisterSubsystems");
    if (reg) {
        reg(&Engine_RegisterFactory);
    }

    m_impl->handles.push_back(h);
    return true;
#endif
}

void PluginManager::UnloadAll() {
    if (!m_impl) return;

#ifdef _WIN32
    for (auto h : m_impl->handles) {
        auto shutdown = (Plugin_Shutdown_Fn)GetProcAddress(h, "Plugin_Shutdown");
        if (shutdown) shutdown();
        FreeLibrary(h);
    }
    m_impl->handles.clear();
#else
    for (auto h : m_impl->handles) {
        auto shutdown = (Plugin_Shutdown_Fn)dlsym(h, "Plugin_Shutdown");
        if (shutdown) shutdown();
        dlclose(h);
    }
    m_impl->handles.clear();
#endif
}

} // namespace Genesis::Engine
