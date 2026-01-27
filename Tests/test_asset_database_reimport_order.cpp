#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <vector>

TEST_CASE("AssetDatabase builds deterministic reimport order", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_reimport_order";
    fs::remove_all(root);
    fs::create_directories(root / "Assets" / "models");
    fs::create_directories(root / "Assets" / "audio");

    fs::path scenePath = root / "Assets" / "scene.scene";
    fs::path modelPath = root / "Assets" / "models" / "a.obj";
    fs::path mtlPath = root / "Assets" / "models" / "a.mtl";
    fs::path texPath = root / "Assets" / "models" / "a_diffuse.png";
    fs::path audioPath = root / "Assets" / "audio" / "a.ogg";

    {
        std::ofstream tex(texPath.string(), std::ios::trunc);
        tex << "PNG";
    }
    {
        std::ofstream mtl(mtlPath.string(), std::ios::trunc);
        mtl << "newmtl mat\nmap_Kd a_diffuse.png\n";
    }
    {
        std::ofstream obj(modelPath.string(), std::ios::trunc);
        obj << "mtllib a.mtl\n";
    }
    {
        std::ofstream audio(audioPath.string(), std::ios::trunc);
        audio << "OGG";
    }
    {
        std::ofstream scene(scenePath.string(), std::ios::trunc);
        scene << "MODEL models/a.obj\n";
        scene << "AUDIO audio/a.ogg\n";
    }

    std::vector<fs::path> roots = { scenePath };
    auto order = Genesis::Engine::AssetDatabase::BuildReimportOrder(roots, root);

    std::vector<std::string> rel;
    rel.reserve(order.size());
    for (const auto& p : order) {
        std::error_code ec;
        auto r = fs::relative(p, root, ec);
        rel.push_back(ec ? p.generic_string() : r.generic_string());
    }

    std::vector<std::string> expected = {
        "Assets/audio/a.ogg",
        "Assets/models/a.mtl",
        "Assets/models/a_diffuse.png",
        "Assets/models/a.obj",
        "Assets/scene.scene"
    };

    REQUIRE(rel == expected);

    fs::remove_all(root);
}
