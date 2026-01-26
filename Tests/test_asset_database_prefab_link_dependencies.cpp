#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>

TEST_CASE("AssetDatabase tracks prefab link dependencies", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_prefab_link";
    fs::remove_all(root);
    fs::create_directories(root / "Assets" / "prefabs");

    fs::path prefabPath = root / "Assets" / "prefabs" / "crate.prefab";
    {
        std::ofstream prefab(prefabPath.string(), std::ios::trunc);
        prefab << "# prefab\n";
    }

    fs::path scenePath = root / "Assets" / "scene.scene";
    {
        std::ofstream scene(scenePath.string(), std::ios::trunc);
        scene << "ENTITY 0\n";
        scene << "PREFAB_LINK prefabs/crate.prefab 0 0 0\n";
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(scenePath, root, &meta));

    REQUIRE(meta.dependencies.size() == 1);
    REQUIRE(meta.dependencies[0] == "Assets/prefabs/crate.prefab");

    fs::remove_all(root);
}
