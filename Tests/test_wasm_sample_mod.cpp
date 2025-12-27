#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"
#include "engine/Engine.h"
#include <fstream>
#include <iterator>

#ifdef HAVE_WASM3
// Minimal wasm module (binary) expressed as hex string. This module imports the 'env' host functions
// and exports a no-op 'mod_init' function. It is tiny and used to validate that the runtime can load
// modules that import host functions and export mod_init.
static const char* kSampleWasmHex =
"0061736d01000000010b026000017f60017f0060027f7f017f03656e760f656e67696e655f6372656174655f626f64790001656e760f656e67696e655f64657374726f795f626f64790002656e760f656e67696e655f6170706c795f696d70756c73650003656e760f656e67696e655f6372656174655f64697374616e63655f6a6f696e740004036d6f645f696e6974000401070b0b03020100";

static std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned hi = 0, lo = 0;
        auto cv = [](char c)->int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
            if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
            return 0;
        };
        hi = cv(hex[i]); lo = cv(hex[i+1]);
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

TEST_CASE("WasmRuntime: load embedded sample module", "[wasm][mod]") {
    REQUIRE(Genesis::Engine::Init() == true);

    // Prefer reading the mod.wasm binary from the repo if present, otherwise fall back to the
    // embedded hex representation (convenience for CI/workspace without the sample files).
    std::filesystem::path binPath = std::filesystem::path(PROJECT_SOURCE_DIR) / "GameProjects" / "SampleGame" / "mods" / "sample_mod" / "mod.wasm";
    std::vector<uint8_t> bytes;
    if (std::filesystem::exists(binPath)) {
        std::ifstream ifs(binPath, std::ios::binary);
        std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        bytes.assign(s.begin(), s.end());
    } else {
        bytes = HexToBytes(kSampleWasmHex);
    }

    REQUIRE(bytes.size() > 0);

    bool ok = Genesis::Engine::WasmRuntime::LoadModuleFromBytes("sample_mod", bytes);
    REQUIRE(ok == true);

    // Call exported mod_init explicitly as well
    REQUIRE(Genesis::Engine::WasmRuntime::CallExported("sample_mod", "mod_init") == true);
}
#else
TEST_CASE("WasmRuntime: sample module not available", "[wasm][mod]") {
    SUCCEED("wasm3 not available; skipping embedded sample mod test");
}
#endif
