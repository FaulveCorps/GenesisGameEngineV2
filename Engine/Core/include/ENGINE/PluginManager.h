#pragma once

#include <string>
#include <vector>

namespace Genesis::Engine {

class PluginManager {
public:
    PluginManager() = default;
    ~PluginManager();

    bool LoadPlugin(const std::string& path);
    void UnloadAll();

private:
    struct Impl;
    Impl* m_impl = nullptr;
};

} // namespace Genesis::Engine
