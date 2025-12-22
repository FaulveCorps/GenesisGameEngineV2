# Plugin API & Guidelines

The engine exposes a minimal C plugin API (`Engine/Core/include/engine/PluginAPI.h`) for runtime modules.

Required exported functions (extern "C"):
- `bool Plugin_Init()` — called when the plugin is loaded; return `true` on success.
- `void Plugin_Shutdown()` — called when the plugin is unloaded.
- `const char* Plugin_Name()` — return a NUL-terminated name string for logging.

Notes:
- Plugins are simple shared libraries that can be loaded with `PluginManager::LoadPlugin(path)`. The sample plugin is in `Plugins/SamplePlugin`.
- Future plugins can register callbacks and extend engine systems; use versioned APIs and semantic version checks to ensure compatibility.
