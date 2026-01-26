#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>

TEST_CASE("AssetDatabase ignores non-file GLTF URIs", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_gltf_uri";
    fs::remove_all(root);
    fs::create_directories(root / "Assets" / "textures");

    fs::path assetPath = root / "Assets" / "model.gltf";
    fs::path texPath = root / "Assets" / "textures" / "diffuse.png";
    {
        std::ofstream tex(texPath.string(), std::ios::trunc);
        tex << "fake";
    }

    {
        std::ofstream gltf(assetPath.string(), std::ios::trunc);
        gltf << "{\n"
             << "  \"buffers\": [{\"uri\":\"data:application/octet-stream;base64,AAAA\"}],\n"
             << "  \"images\": [{\"uri\":\"textures/diffuse.png\"}]\n"
             << "}\n";
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(assetPath, root, &meta));
    REQUIRE(meta.importer == "model");

    REQUIRE(meta.dependencies.size() == 1);
    REQUIRE(meta.dependencies[0] == "Assets/textures/diffuse.png");

    fs::remove_all(root);
}
