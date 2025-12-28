#include "catch_amalgamated.hpp"
#include "engine/WasmRuntime.h"
#include <sstream>
#include <iostream>
#include <thread>

#ifdef HAVE_WASM3

// Minimal wasm module that imports 'env::test_sleep' (v()) and exports 'run' which calls it.
static const char* kSleepWasmHex =
"0061736d01000000"
"010401600000"
"02120103656e760a746573745f736c6565700000"
"03020100"
"0707010372756e0001"
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

// Host sleep callback used by the test; sleeps longer than the call timeout
m3ApiRawFunction(test_sleep_cb) {
    // Sleep for a while to simulate a long-running guest
    std::this_thread::sleep_for(std::chrono::milliseconds(3000));
    m3ApiSuccess();
}

TEST_CASE("WasmRuntime: execution timeout and module quarantine", "[wasm][limits]") {
    REQUIRE(Genesis::Engine::WasmRuntime::Init() == true);

    auto token = Genesis::Engine::WasmRuntime::RegisterHostFunction("env", "test_sleep", "v()", test_sleep_cb);
    REQUIRE(token.valid());

    auto bytes = HexToBytes(std::string(kSleepWasmHex));
    REQUIRE(bytes.size() > 0);

    // Load the module (it does not call mod_init automatically)
    REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("sleep_mod", bytes) == true);

    std::ostringstream errbuf;
    auto oldcerr = std::cerr.rdbuf(errbuf.rdbuf());

    // Call with a very small timeout; the host callback sleeps for 3000ms so this should time out
    bool ok = Genesis::Engine::WasmRuntime::CallExportedWithTimeout("sleep_mod", "run", {}, 100);
    REQUIRE(ok == false);

    // Subsequent calls should be rejected (module marked timed-out)
    bool ok2 = Genesis::Engine::WasmRuntime::CallExported("sleep_mod", "run");
    REQUIRE(ok2 == false);

    std::string logs = errbuf.str();
    REQUIRE(logs.find("timed out") != std::string::npos);
    REQUIRE(logs.find("sleep_mod") != std::string::npos);
    REQUIRE(logs.find("run") != std::string::npos);

    std::cerr.rdbuf(oldcerr);
}

#else
TEST_CASE("WasmRuntime: limits not available", "[wasm][limits]") {
    SUCCEED("wasm3 not available; skipping wasm limits test");
}
#endif
