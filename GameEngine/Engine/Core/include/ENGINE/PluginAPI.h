#pragma once

// Simple, versioned C plugin API for runtime modules (mods/plugins)

#ifdef _WIN32
#  ifdef PLUGIN_EXPORT
#    define PLUGIN_API __declspec(dllexport)
#  else
#    define PLUGIN_API __declspec(dllimport)
#  endif
#else
#  define PLUGIN_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

// Plugin API version
#define GENESIS_PLUGIN_API_MAJOR 1
#define GENESIS_PLUGIN_API_MINOR 0

// Required plugin functions (extern "C"):
// bool Plugin_Init();
// void Plugin_Shutdown();
// const char* Plugin_Name();

typedef bool (*Plugin_Init_Fn)();
typedef void (*Plugin_Shutdown_Fn)();
typedef const char* (*Plugin_Name_Fn)();

// Optional plugin registration hook (extern "C").
// If present, the engine will call this function after Plugin_Init to allow the
// plugin to register subsystem factories. Signature:
//   void Plugin_RegisterSubsystems(void (*engine_register)(const char* subsystemType, const char* name, void* (*factory)()))
typedef void (*Plugin_RegisterSubsystems_Fn)(void (*engine_register)(const char* subsystemType, const char* name, void* (*factory)()));

#ifdef __cplusplus
}
#endif
