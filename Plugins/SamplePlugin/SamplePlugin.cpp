#include "engine/PluginAPI.h"
#include <iostream>

extern "C" PLUGIN_API bool Plugin_Init() {
    std::cout << "SamplePlugin: Plugin_Init called" << std::endl;
    return true;
}

extern "C" PLUGIN_API void Plugin_Shutdown() {
    std::cout << "SamplePlugin: Plugin_Shutdown called" << std::endl;
}

extern "C" PLUGIN_API const char* Plugin_Name() {
    return "SamplePlugin v1.0";
}
