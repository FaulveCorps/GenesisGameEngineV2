# Plugin API & Guidelines

The engine exposes a minimal C plugin API (`Engine/Core/include/Engine/PluginAPI.h`) for runtime modules.

Required exported functions (extern "C"):
- `bool Plugin_Init()` — called when the plugin is loaded; return `true` on success.
- `void Plugin_Shutdown()` — called when the plugin is unloaded.
- `const char* Plugin_Name()` — return a NUL-terminated name string for logging.

Notes:
- Plugins are simple shared libraries that can be loaded with `PluginManager::LoadPlugin(path)`. The sample plugin is in `Plugins/SamplePlugin`.
- Future plugins can register callbacks and extend engine systems; use versioned APIs and semantic version checks to ensure compatibility.
## Subsystem registration (optional)

Plugins may optionally export a registration hook so they can register custom subsystem backends (Audio, Physics, Network, etc.). `PluginManager` will call this function after `Plugin_Init` if present:

```cpp
// Signature (C linkage):
void Plugin_RegisterSubsystems(void (*engine_register)(const char* subsystemType, const char* name, void* (*factory)()));
```

The plugin should call `engine_register` for each backend it exposes. The `factory` must be a C-callable function that returns a pointer to a heap-allocated object derived from `Genesis::Engine::ISubsystem` (cast to `void*`). The engine will take ownership of the returned pointer.

Example (plugin side, simplified):

```cpp
extern "C" PLUGIN_API void* MyAudio_Create() { return new MyAudio(); }

extern "C" PLUGIN_API void Plugin_RegisterSubsystems(void (*registerFactory)(const char*, const char*, void* (*)())) {
    registerFactory("Audio", "myaudio", &MyAudio_Create);
}
```

See `Engine/Core/include/ENGINE/SubsystemRegistry.h` for the runtime registry the engine uses to store factories.