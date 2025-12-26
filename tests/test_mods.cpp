#include "catch_amalgamated.hpp"
#include "engine/ModManager.h"
#include <filesystem>
#include <fstream>

using namespace Genesis::Engine;

static std::string UniqueTempModsDir() {
    auto tmp = std::filesystem::temp_directory_path();
    tmp /= ("gen_mods_test_" + std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));
    return tmp.string();
}

TEST_CASE("ModManager: scan mods with manifest and without", "[mods]") {
    auto tmp = UniqueTempModsDir();
    std::filesystem::create_directories(tmp);

    // Create mod with manifest
    std::filesystem::create_directories(tmp + "/modA");
    std::ofstream ofs(tmp + "/modA/mod.json");
    ofs << "{ \"id\": \"mod_a\", \"name\": \"Mod A\", \"version\": \"1.0\", \"description\": \"Test mod A\" }";
    ofs.close();

    // Create mod without manifest
    std::filesystem::create_directories(tmp + "/modB");

    ModManager mm;
    REQUIRE(mm.Scan(tmp) == true);

    auto mods = mm.Mods();
    REQUIRE(mods.size() == 2);

    // modA should be present
    bool foundA = false;
    for (auto &m : mods) {
        if (m.id == "mod_a") {
            foundA = true;
            REQUIRE(m.name == "Mod A");
            REQUIRE(m.version == "1.0");
            REQUIRE(m.description == "Test mod A");
        }
    }
    REQUIRE(foundA);

    // cleanup
    std::filesystem::remove_all(tmp);
}
