#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"
#include <fstream>

#ifdef HAVE_WASM3
TEST_CASE("WasmRuntime: load sample mod from repo hex file", "[wasm][mod][file]") {
    std::string path = std::string(PROJECT_SOURCE_DIR) + "/GameProjects/SampleGame/mods/sample_mod/mod.wasm.hex";
    if (!std::filesystem::exists(path)) {
        WARN("No sample mod hex file found; skipping") ;
        return;
    }
    std::ifstream ifs(path);
    REQUIRE(ifs.good());
    std::string hex((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
    std::string cleaned;
    for (char c : hex) if (!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);
    std::vector<uint8_t> bytes;
    for (size_t i=0;i+1<cleaned.size(); i+=2) {
        auto cv = [](char c)->int{ if (c>='0'&&c<='9') return c-'0'; if (c>='a'&&c<='f') return 10 + (c - 'a'); if (c>='A'&&c<='F') return 10 + (c - 'A'); return 0; };
        uint8_t b = static_cast<uint8_t>((cv(cleaned[i]) << 4) | cv(cleaned[i+1]));
        bytes.push_back(b);
    }

    REQUIRE(bytes.size() > 0);
    REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("sample_mod_file", bytes) == true);
}
#else
TEST_CASE("WasmRuntime: sample mod file not available", "[wasm][mod][file]") {
    SUCCEED("wasm3 not available; skipping file load test");
}
#endif
