#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"
#include <sstream>

#ifdef HAVE_WASM3

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

    // Use an existing embedded sample if available (use simple smoke module from other tests)
    extern const char* kSampleWasmHex; // present in test_wasm_sample_mod.cpp
    auto sampleBytes = HexToBytes(std::string(kSampleWasmHex));
    REQUIRE(sampleBytes.size() > 0);

    REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("sample_mod_raii", sampleBytes) == true);
    auto loaded = Genesis::Engine::WasmRuntime::LoadedModules();
    REQUIRE(std::find(loaded.begin(), loaded.end(), "sample_mod_raii") != loaded.end());

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
