#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>

TEST_CASE("AssetDatabase decodes percent-encoded GLTF URIs", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_gltf_decode";
    fs::remove_all(root);
    fs::create_directories(root / "Assets" / "textures");

    fs::path assetPath = root / "Assets" / "model.gltf";
    fs::path texPath = root / "Assets" / "textures" / "diffuse space.png";
    {
        std::ofstream tex(texPath.string(), std::ios::trunc);
        tex << "fake";
    }

    {
        std::ofstream gltf(assetPath.string(), std::ios::trunc);
        gltf << "{\n"
             << "  \"images\": [{\"uri\":\"textures/diffuse%20space.png\"}]\n"
             << "}\n";
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(assetPath, root, &meta));
    REQUIRE(meta.importer == "model");

    REQUIRE(meta.dependencies.size() == 1);
    REQUIRE(meta.dependencies[0] == "Assets/textures/diffuse space.png");

    fs::remove_all(root);
}
