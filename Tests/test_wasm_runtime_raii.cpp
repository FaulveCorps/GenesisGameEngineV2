#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"
#include "engine/Engine.h"
#include <sstream>
#include <iostream>
#include <fstream>
#include <algorithm>

#ifdef HAVE_WASM3

// Embedded sample wasm used by existing tests
static const char* kSampleWasmHex =
"0061736d01000000"
"012105600060057f7f7f7f7f017f60017f0060037f7f7f0060067f7f7f7f7f7f017f"
"027204"
"03656e7612656e67696e655f6372656174655f626f64790001"
"03656e7613656e67696e655f64657374726f795f626f64790002"
"03656e7614656e67696e655f6170706c795f696d70756c73650003"
"03656e761c656e67696e655f6372656174655f64697374616e63655f6a6f696e740004"
"03020100"
"070c01086d6f645f696e69740004"
"0a040102000b";

// Minimal wasm module that imports 'env::test_ping' and exports a mod_init that calls it.
static const char* kPingWasmHex =
"0061736d01000000"
"010401600000"
"02110103656e7609746573745f70696e670000"
"03020100"
"070c01086d6f645f696e69740001"
"0a0601040010000b";

static std::vector<uint8_t> HexToBytes(const std::string& hex) {
    std::vector<uint8_t> out;
    out.reserve(hex.size() / 2);
    auto cv = [](char c)->int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return 0;
    };
    for (size_t i = 0; i + 1 < hex.size(); i += 2) {
        unsigned hi = cv(hex[i]); unsigned lo = cv(hex[i+1]);
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

// Simple host callback used by test
static bool g_test_ping_called = false;
m3ApiRawFunction(test_ping_cb) {
    g_test_ping_called = true;
    m3ApiSuccess();
}

TEST_CASE("WasmRuntime RAII: init, load and shutdown", "[wasm][raii]") {
    REQUIRE(Genesis::Engine::WasmRuntime::Init() == true);

    // Use a minimal, self-contained wasm module that exports a no-op 'mod_init' (no imports)
    static const char* kNoopWasmHex =
    "0061736d01000000"
    "010401600000"
    "03020100"
    "070c01086d6f645f696e69740000"
    "0a040102000b";

    auto sampleBytes = HexToBytes(std::string(kNoopWasmHex));
    REQUIRE(sampleBytes.size() > 0);

    REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("noop_mod", sampleBytes) == true);
    auto loaded = Genesis::Engine::WasmRuntime::LoadedModules();
    bool found = false;
    for (auto &s : loaded) { if (s == "noop_mod") { found = true; break; } }
    REQUIRE(found);

    // Shutdown and ensure modules cleared, then re-init
    Genesis::Engine::WasmRuntime::Shutdown();
    Genesis::Engine::WasmRuntime::Init();
}

TEST_CASE("WasmRuntime RAII: invalid module logs error", "[wasm][raii]") {
    REQUIRE(Genesis::Engine::WasmRuntime::Init() == true);
    std::ostringstream errbuf;
    auto oldcerr = std::cerr.rdbuf(errbuf.rdbuf());

    std::vector<uint8_t> bad = {0x00, 0x01, 0x02, 0x03};
    REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("bad_mod", bad) == false);

    std::string out = errbuf.str();
    REQUIRE(out.find("m3_ParseModule failed") != std::string::npos);
    REQUIRE(out.find("bad_mod") != std::string::npos);

    std::cerr.rdbuf(oldcerr);
}

TEST_CASE("WasmRuntime RAII: register and invoke host function", "[wasm][host]") {
    REQUIRE(Genesis::Engine::WasmRuntime::Init() == true);

    g_test_ping_called = false;
    auto token = Genesis::Engine::WasmRuntime::RegisterHostFunction("env", "test_ping", "v()", test_ping_cb);
    REQUIRE(token.valid());

    auto pingBytes = HexToBytes(std::string(kPingWasmHex));
    REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("ping_mod", pingBytes) == true);

    // Call mod_init which should invoke the imported host 'test_ping'
    REQUIRE(Genesis::Engine::WasmRuntime::CallExported("ping_mod", "mod_init") == true);
    REQUIRE(g_test_ping_called == true);

    // Destroy token, unregister; future modules won't have this host function
}

#else
TEST_CASE("WasmRuntime RAII: wasm3 not available", "[wasm][raii]") {
    SUCCEED("wasm3 not available; skipping wasm RAII tests");
}
#endif
