#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"

#ifdef HAVE_WASM3
#include <fstream>
#include <iostream>

TEST_CASE("WasmRuntime: load sample mod wasm binary (e2e)", "[wasm][mod][e2e]") {
    REQUIRE(Genesis::Engine::Init() == true);

    namespace fs = std::filesystem;
    fs::path p = fs::path(PROJECT_SOURCE_DIR) / "GameProjects" / "SampleGame" / "mods" / "sample_mod" / "mod.wasm";

    if (!fs::exists(p)) {
        // Try to generate a temporary wasm file from the repo hex file
        fs::path hexp = fs::path(PROJECT_SOURCE_DIR) / "GameProjects" / "SampleGame" / "mods" / "sample_mod" / "mod.wasm.hex";
        if (!fs::exists(hexp)) {
            WARN("No sample mod binary or hex file found; skipping e2e test");
            return;
        }

        std::ifstream ifs(hexp);
        REQUIRE(ifs.good());
        std::string hex((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string cleaned;
        for (char c : hex) if (!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);

        std::vector<uint8_t> bytes;
        for (size_t i = 0; i + 1 < cleaned.size(); i += 2) {
            auto cv = [](char c)->int { if (c >= '0' && c <= '9') return c - '0'; if (c >= 'a' && c <= 'f') return 10 + (c - 'a'); if (c >= 'A' && c <= 'F') return 10 + (c - 'A'); return 0; };
            uint8_t b = static_cast<uint8_t>((cv(cleaned[i]) << 4) | cv(cleaned[i+1]));
            bytes.push_back(b);
        }

        REQUIRE(bytes.size() > 0);

        fs::path tmp = fs::temp_directory_path() / "sample_mod.wasm";
        std::ofstream ofs(tmp, std::ios::binary);
        REQUIRE(ofs.good());
        ofs.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
        ofs.close();
        p = tmp;
    }

    REQUIRE(Genesis::Engine::WasmRuntime::LoadModule(p) == true);

    // LoadModule calls mod_init automatically if present; additionally verify we can call it explicitly
    std::string moduleName = p.filename().string();
    REQUIRE(Genesis::Engine::WasmRuntime::CallExported(moduleName, "mod_init") == true);
}
#else
TEST_CASE("WasmRuntime e2e not available", "[wasm][mod][e2e]") {
    SUCCEED("wasm3 not available; skipping wasm e2e test");
}
#endif
