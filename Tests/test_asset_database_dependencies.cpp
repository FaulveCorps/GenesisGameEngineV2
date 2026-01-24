#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <chrono>

TEST_CASE("AssetDatabase detects dependency timestamp changes", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_dep_test";
    fs::remove_all(root);
    fs::create_directories(root / "Assets");

    fs::path scenePath = root / "Assets" / "scene.scene";
    fs::path modelPath = root / "Assets" / "model.obj";

    {
        std::ofstream model(modelPath.string(), std::ios::trunc);
        model << "# dummy obj\n";
    }

    {
        std::ofstream scene(scenePath.string(), std::ios::trunc);
        scene << "MODEL model.obj\n";
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(scenePath, root, &meta));
    REQUIRE_FALSE(Genesis::Engine::AssetDatabase::DependenciesChanged(meta, root));

    auto future = fs::file_time_type::clock::now() + std::chrono::seconds(2);
    fs::last_write_time(modelPath, future);

    REQUIRE(Genesis::Engine::AssetDatabase::DependenciesChanged(meta, root));

    fs::remove_all(root);
}
