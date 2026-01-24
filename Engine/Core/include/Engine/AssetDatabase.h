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
    struct DependencyStamp {
        std::string path;
        uint64_t timestamp = 0;
    };
    std::vector<DependencyStamp> dependencyTimestamps;
    struct ImportSetting {
        std::string key;
        std::string value;
    };
    std::vector<ImportSetting> importSettings;
};

class AssetDatabase {
public:
    static std::filesystem::path GetMetaPath(const std::filesystem::path& assetPath);
    static bool LoadMeta(const std::filesystem::path& assetPath, AssetMeta& outMeta);
    static bool SaveMeta(const std::filesystem::path& assetPath, const AssetMeta& meta);
    static AssetMeta EnsureMeta(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot);
    static bool Reimport(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot, AssetMeta* outMeta = nullptr);
    static bool IsMetaFile(const std::filesystem::path& path);
    static bool GetSourceTimestamp(const std::filesystem::path& assetPath, uint64_t& outTimestamp);
    static bool DependenciesChanged(const AssetMeta& meta, const std::filesystem::path& projectRoot);
    static void SetImportSetting(AssetMeta& meta, const std::string& key, const std::string& value);
    static bool GetImportSetting(const AssetMeta& meta, const std::string& key, std::string& outValue);
    static void RemoveImportSetting(AssetMeta& meta, const std::string& key);
};

} // namespace Genesis::Engine
