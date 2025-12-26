#include "engine/ModManager.h"
#include <fstream>
#include <iostream>
#include <algorithm>

// Use nlohmann::json if available; otherwise use a simple fallback parser for minimal manifest fields
#ifdef HAVE_NLOHMANN_JSON
#include <nlohmann/json.hpp>
#endif

namespace Genesis::Engine {

static std::string ReadFileToString(const std::filesystem::path& p) {
    std::ifstream ifs(p);
    if (!ifs) return {};
    std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    return s;
}

static std::string Trim(const std::string& s) {
    size_t a = s.find_first_not_of(" \t\n\r");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\n\r");
    return s.substr(a, b - a + 1);
}

bool ModManager::Scan(const std::filesystem::path& modsDir) {
    m_mods.clear();
    try {
        if (!std::filesystem::exists(modsDir) || !std::filesystem::is_directory(modsDir)) return false;
        for (auto& entry : std::filesystem::directory_iterator(modsDir)) {
            if (!entry.is_directory()) continue;
            ModInfo info;
            info.path = entry.path();
            info.id = entry.path().filename().string();
            info.name = info.id;
            info.version = "";
            info.description = "";

            auto manifest = entry.path() / "mod.json";
            if (std::filesystem::exists(manifest) && std::filesystem::is_regular_file(manifest)) {
                auto txt = ReadFileToString(manifest);
#ifdef HAVE_NLOHMANN_JSON
                try {
                    auto j = nlohmann::json::parse(txt);
                    if (j.contains("id")) info.id = j["id"].get<std::string>();
                    if (j.contains("name")) info.name = j["name"].get<std::string>();
                    if (j.contains("version")) info.version = j["version"].get<std::string>();
                    if (j.contains("description")) info.description = j["description"].get<std::string>();
                    if (j.contains("language")) info.language = j["language"].get<std::string>();
                } catch (...) {
                    std::cerr << "ModManager: failed to parse manifest " << manifest.string() << std::endl;
                }
#else
                // Very small heuristic parser: look for "key": "value" patterns for id/name/version/description.
                auto find_str = [&](const std::string& key)->std::string{
                    std::string pat = "\"" + key + "\"";
                    auto pos = txt.find(pat);
                    if (pos == std::string::npos) return std::string();
                    auto colon = txt.find(':', pos);
                    if (colon == std::string::npos) return std::string();
                    auto start = txt.find('"', colon);
                    if (start == std::string::npos) return std::string();
                    start++;
                    auto end = txt.find('"', start);
                    if (end == std::string::npos) return std::string();
                    return Trim(txt.substr(start, end - start));
                };
                auto id = find_str("id"); if (!id.empty()) info.id = id;
                auto name = find_str("name"); if (!name.empty()) info.name = name;
                auto version = find_str("version"); if (!version.empty()) info.version = version;
                auto desc = find_str("description"); if (!desc.empty()) info.description = desc;
                auto lang = find_str("language"); if (!lang.empty()) info.language = lang;
#endif
            }
            m_mods.push_back(info);
        }
    } catch (const std::exception& e) {
        std::cerr << "ModManager: Scan failed: " << e.what() << std::endl;
        return false;
    }
    // sort mods by id for deterministic output
    std::sort(m_mods.begin(), m_mods.end(), [](const ModInfo& a, const ModInfo& b){ return a.id < b.id; });
    return true;
}

} // namespace Genesis::Engine
