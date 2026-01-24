#include "catch_amalgamated.hpp"
#include "engine/AssetDatabase.h"
#include <filesystem>
#include <fstream>
#include <vector>

TEST_CASE("AssetDatabase stores dependencies in deterministic order", "[assetdb]") {
    namespace fs = std::filesystem;

    fs::path root = fs::temp_directory_path() / "genesis_assetdb_dep_order";
    fs::remove_all(root);
    fs::create_directories(root / "Assets" / "models");
    fs::create_directories(root / "Assets" / "audio");

    fs::path scenePath = root / "Assets" / "scene.scene";
    fs::path modelA = root / "Assets" / "models" / "a.obj";
    fs::path modelB = root / "Assets" / "models" / "b.obj";
    fs::path audioA = root / "Assets" / "audio" / "a.ogg";
    fs::path audioZ = root / "Assets" / "audio" / "z.ogg";

    {
        std::ofstream f(modelA.string(), std::ios::trunc);
        f << "# a";
    }
    {
        std::ofstream f(modelB.string(), std::ios::trunc);
        f << "# b";
    }
    {
        std::ofstream f(audioA.string(), std::ios::trunc);
        f << "# a";
    }
    {
        std::ofstream f(audioZ.string(), std::ios::trunc);
        f << "# z";
    }

    {
        std::ofstream scene(scenePath.string(), std::ios::trunc);
        scene << "MODEL models/b.obj\n";
        scene << "AUDIO audio/z.ogg\n";
        scene << "MODEL models/a.obj\n";
        scene << "AUDIO audio/a.ogg\n";
    }

    Genesis::Engine::AssetMeta meta;
    REQUIRE(Genesis::Engine::AssetDatabase::Reimport(scenePath, root, &meta));

    std::vector<std::string> expected = {
        "Assets/audio/a.ogg",
        "Assets/audio/z.ogg",
        "Assets/models/a.obj",
        "Assets/models/b.obj"
    };

    REQUIRE(meta.dependencies == expected);

    fs::remove_all(root);
}
