#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>

TEST_CASE("AssetDatabase import settings roundtrip", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_settings";
    fs::remove_all(root);
    fs::create_directories(root / "Assets");

    fs::path assetPath = root / "Assets" / "texture.png";
    {
        std::ofstream asset(assetPath.string(), std::ios::trunc);
        asset << "fake";
    }

    Genesis::Engine::AssetMeta meta;
    meta.guid = "test-guid";
    meta.importer = "texture";
    meta.sourceTimestamp = 0;
    Genesis::Engine::AssetDatabase::SetImportSetting(meta, "wrap", "repeat");
    Genesis::Engine::AssetDatabase::SetImportSetting(meta, "filter", "linear");

    REQUIRE(Genesis::Engine::AssetDatabase::SaveMeta(assetPath, meta));

    Genesis::Engine::AssetMeta loaded;
    REQUIRE(Genesis::Engine::AssetDatabase::LoadMeta(assetPath, loaded));

    std::string wrap;
    std::string filter;
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(loaded, "wrap", wrap));
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(loaded, "filter", filter));
    REQUIRE(wrap == "repeat");
    REQUIRE(filter == "linear");

    Genesis::Engine::AssetDatabase::RemoveImportSetting(loaded, "wrap");
    REQUIRE_FALSE(Genesis::Engine::AssetDatabase::GetImportSetting(loaded, "wrap", wrap));

    fs::remove_all(root);
}
