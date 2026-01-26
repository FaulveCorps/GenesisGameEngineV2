#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>

TEST_CASE("AssetDatabase model import defaults", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_model_defaults";
    fs::remove_all(root);
    fs::create_directories(root / "Assets");

    fs::path assetPath = root / "Assets" / "model.obj";
    {
        std::ofstream asset(assetPath.string(), std::ios::trunc);
        asset << "o dummy\n";
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(assetPath, root, &meta));
    REQUIRE(meta.importer == "model");

    std::string value;
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(meta, "gen_normals", value));
    REQUIRE(value == "1");
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(meta, "flip_uvs", value));
    REQUIRE(value == "0");
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(meta, "optimize_meshes", value));
    REQUIRE(value == "1");
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(meta, "pretransform_vertices", value));
    REQUIRE(value == "0");
    REQUIRE(Genesis::Engine::AssetDatabase::GetImportSetting(meta, "scale_factor", value));
    REQUIRE(value == "1.0");

    fs::remove_all(root);
}
