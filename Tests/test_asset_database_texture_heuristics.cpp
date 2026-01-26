#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>

TEST_CASE("AssetDatabase texture heuristics set normal/data defaults", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_tex_heuristics";
    fs::remove_all(root);
    fs::create_directories(root / "Assets");

    fs::path normalPath = root / "Assets" / "wall_normal.png";
    fs::path roughPath = root / "Assets" / "metallic_roughness.png";

    {
        std::ofstream f(normalPath.string(), std::ios::trunc);
        f << "fake";
    }
    {
        std::ofstream f(roughPath.string(), std::ios::trunc);
        f << "fake";
    }

    Genesis::Engine::AssetMeta metaNormal;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(normalPath, root, &metaNormal));

    std::string value;
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(metaNormal, "normal_map", value));
    REQUIRE(value == "1");
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(metaNormal, "srgb", value));
    REQUIRE(value == "0");

    Genesis::Engine::AssetMeta metaRough;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(roughPath, root, &metaRough));
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(metaRough, "srgb", value));
    REQUIRE(value == "0");

    fs::remove_all(root);
}
