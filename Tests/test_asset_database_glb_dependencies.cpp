#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdint>

static void WriteU32LE(std::vector<uint8_t>& out, uint32_t value) {
    out.push_back(static_cast<uint8_t>(value & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<uint8_t>((value >> 24) & 0xFF));
}

TEST_CASE("AssetDatabase parses GLB dependencies", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_glb_dep";
    fs::remove_all(root);
    fs::create_directories(root / "Assets" / "textures");

    fs::path assetPath = root / "Assets" / "model.glb";
    fs::path texPath = root / "Assets" / "textures" / "diffuse.png";
    {
        std::ofstream tex(texPath.string(), std::ios::trunc);
        tex << "fake";
    }

    std::string json =
        "{"
        "\"buffers\":[{\"byteLength\":0,\"uri\":\"data:application/octet-stream;base64,AAAA\"}],"
        "\"images\":[{\"uri\":\"textures/diffuse.png\"}]"
        "}";

    const size_t jsonLen = json.size();
    const size_t paddedLen = (jsonLen + 3) & ~static_cast<size_t>(3);
    json.append(paddedLen - jsonLen, ' ');

    std::vector<uint8_t> glb;
    glb.reserve(12 + 8 + paddedLen);
    WriteU32LE(glb, 0x46546C67); // 'glTF'
    WriteU32LE(glb, 2);
    WriteU32LE(glb, static_cast<uint32_t>(12 + 8 + paddedLen));

    WriteU32LE(glb, static_cast<uint32_t>(paddedLen));
    WriteU32LE(glb, 0x4E4F534A); // 'JSON'
    glb.insert(glb.end(), json.begin(), json.end());

    {
        std::ofstream out(assetPath, std::ios::binary | std::ios::trunc);
        out.write(reinterpret_cast<const char*>(glb.data()), static_cast<std::streamsize>(glb.size()));
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(assetPath, root, &meta));
    REQUIRE(meta.importer == "model");

    REQUIRE(meta.dependencies.size() == 1);
    REQUIRE(meta.dependencies[0] == "Assets/textures/diffuse.png");

    fs::remove_all(root);
}
