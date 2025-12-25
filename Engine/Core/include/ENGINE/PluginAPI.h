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

#ifdef __cplusplus
}
#endif
