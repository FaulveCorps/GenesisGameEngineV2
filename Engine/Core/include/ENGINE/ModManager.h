#pragma once

#include <string>
#include <vector>
#include <filesystem>

namespace Genesis::Engine {

struct ModInfo {
    std::string id;
    std::string name;
    std::string version;
    std::string description;
    std::filesystem::path path;
};

class ModManager {
public:
    ModManager() = default;

    // Scan the given mods directory and populate the list of discovered mods.
    // Returns true on success (directory exists and was scanned) or false on error.
    bool Scan(const std::filesystem::path& modsDir);

    const std::vector<ModInfo>& Mods() const { return m_mods; }

private:
    std::vector<ModInfo> m_mods;
};

} // namespace Genesis::Engine
