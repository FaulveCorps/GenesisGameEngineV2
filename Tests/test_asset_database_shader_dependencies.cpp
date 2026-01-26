#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>

TEST_CASE("AssetDatabase parses shader include dependencies", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_shader_dep";
    fs::remove_all(root);
    fs::create_directories(root / "Assets" / "shaders" / "includes");

    fs::path includePath = root / "Assets" / "shaders" / "includes" / "common.glsl";
    {
        std::ofstream inc(includePath.string(), std::ios::trunc);
        inc << "// common";
    }

    fs::path shaderPath = root / "Assets" / "shaders" / "test.vert";
    {
        std::ofstream shader(shaderPath.string(), std::ios::trunc);
        shader << "#version 330 core\n";
        shader << "#include \"includes/common.glsl\"\n";
        shader << "void main() {}\n";
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(shaderPath, root, &meta));
    REQUIRE(meta.importer == "shader");

    REQUIRE(meta.dependencies.size() == 1);
    REQUIRE(meta.dependencies[0] == "Assets/shaders/includes/common.glsl");

    fs::remove_all(root);
}
