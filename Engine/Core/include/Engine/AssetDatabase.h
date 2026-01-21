#pragma once

#include <string>
#include <vector>
#include <filesystem>
#include <cstdint>

namespace Genesis::Engine {

struct AssetMeta {
    std::string guid;
    std::string importer;
    uint64_t sourceTimestamp = 0;
    std::vector<std::string> dependencies;
};

class AssetDatabase {
public:
    static std::filesystem::path GetMetaPath(const std::filesystem::path& assetPath);
    static bool LoadMeta(const std::filesystem::path& assetPath, AssetMeta& outMeta);
    static bool SaveMeta(const std::filesystem::path& assetPath, const AssetMeta& meta);
    static AssetMeta EnsureMeta(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot);
    static bool Reimport(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot, AssetMeta* outMeta = nullptr);
    static bool IsMetaFile(const std::filesystem::path& path);
};

} // namespace Genesis::Engine
