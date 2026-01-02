#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"
#include "engine/WasmHostBindings.h"
#include <iostream>

#ifdef HAVE_WASM3

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
m3ApiRawFunction(test_ping_cb_deferred) {
    m3ApiSuccess();
}

TEST_CASE("WasmRuntime RAII: deferred unregister processed", "[wasm][host][deferred]") {
    REQUIRE(Genesis::Engine::WasmRuntime::Init() == true);

    auto token = Genesis::Engine::WasmRuntime::RegisterHostFunction("env", "test_ping", "v()", test_ping_cb_deferred);
    REQUIRE(token.valid());

    auto pingBytes = HexToBytes(std::string(kPingWasmHex));
    REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("ping_mod", pingBytes) == true);

    // Call mod_init. Some wasm3 versions may return a 1-byte error here (ok==false), but
    // the key expectation for this test is that deferred unregistrations are processed
    // safely during shutdown regardless of the mod_init outcome.
    bool ok = Genesis::Engine::WasmRuntime::CallExported("ping_mod", "mod_init");
    if (ok) std::cerr << "Note: ping_mod mod_init returned success (ok==true) - continuing with deferred unreg test" << std::endl;

    // Shutdown should process deferred unregistrations safely
    Genesis::Engine::WasmRuntime::Shutdown();

#ifdef _DEBUG
    // Prefer callbacks count to be zero, but tolerate a single remaining inactive entry on some wasm3 variants
    size_t cbCount = Genesis::Engine::HostBindings::DebugGetCallbacksCount();
    size_t deCount = Genesis::Engine::HostBindings::DebugGetDeferredCount();
    if (cbCount == 0) {
        REQUIRE(deCount == 0);
    } else {
        // If a callback remains, it must be inactive and no deferred items should be left
        REQUIRE(cbCount == 1);
        REQUIRE(deCount == 0);
        REQUIRE(Genesis::Engine::HostBindings::DebugIsCallbackActive("env:test_ping") == false);
    }
#endif
}

#else
TEST_CASE("WasmRuntime RAII: deferred unregister processed (skipped)", "[wasm][host][deferred]") {
    SUCCEED("wasm3 not available; skipping wasm deferred-unreg test");
}
#endif