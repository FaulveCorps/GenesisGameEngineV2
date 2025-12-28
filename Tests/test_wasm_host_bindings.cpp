#include "catch_amalgamated.hpp"
#include <iostream>

#ifdef HAVE_WASM3

#include "engine/WasmHostBindings.h"
#include "wasm3.h"
#include "engine/WasmRuntime.h"
#include <thread>
#include <chrono>

#ifdef HAVE_WASM3
// Test helper: a raw host function that blocks for a while to simulate a long-running host
m3ApiRawFunction(test_host_sleep) {
    m3ApiGetArg(int32_t, ptr);
    m3ApiGetArg(int32_t, len);
    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    m3ApiSuccess();
}
#endif

#include <filesystem>
#include <fstream>
#include <iterator>
#include <vector>
#include <string>

// If CI doesn't provide a precompiled `host_log.wasm`, this hex is a small fallback
// compiled from `host_log.wat` (module imports `env.host_log` and exports `run` which
// calls it with the data segment "hello").
static const char* kHostLogWasmHex =
"0061736d0100000001090260027f7f0060000002100103656e7608686f73745f6c6f670000030201010503010001071002066d656d6f727902000372756e00010a0a0108004100410510000b0b0b010041000b0568656c6c6f";

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
        unsigned hi = cv(hex[i]);
        unsigned lo = cv(hex[i+1]);
        out.push_back(static_cast<uint8_t>((hi << 4) | lo));
    }
    return out;
}

TEST_CASE("Wasm HostBindings: host_log receives string from module", "[wasm][host]") {
    std::cerr << "Test: start host_log test" << std::endl;
    // Prefer a repo-provided wasm binary; if not present, fall back to the embedded hex.
    std::filesystem::path binPath = std::filesystem::path(PROJECT_SOURCE_DIR) / "Tests" / "data" / "wasm" / "host_bindings" / "host_log.wasm";

    std::vector<uint8_t> bytes;
    if (std::filesystem::exists(binPath)) {
        std::ifstream ifs(binPath, std::ios::binary);
        std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        bytes.assign(s.begin(), s.end());
    } else {
        // Fallback to embedded hex; in CI it's preferred to provide the .wasm or have wat2wasm
        // available at configure time to produce the binary from the .wat file in Tests/data.
        bytes = HexToBytes(kHostLogWasmHex);
    }

    REQUIRE(bytes.size() > 0);

    // Parse module and create a dedicated runtime (we don't use WasmRuntime helpers here to keep the test focused)
    IM3Environment env = m3_NewEnvironment();
    REQUIRE(env != nullptr);

    IM3Module module = nullptr;
    std::cerr << "Test: about to parse module, bytes=" << bytes.size() << std::endl;
    M3Result r = m3_ParseModule(env, &module, bytes.data(), bytes.size());
    REQUIRE(r == m3Err_none);
    std::cerr << "Test: parsed module, bytes=" << bytes.size() << " module=" << module << std::endl;

    IM3Runtime runtime = nullptr;

    // Register a host function that captures the string passed from wasm
    Genesis::Engine::HostBindings hb(module);
    std::string captured;
    {
        // Now attach a runtime and load the module (linking host imports requires the module to be loaded into a runtime)
        std::cerr << "Test: creating runtime" << std::endl;
        runtime = m3_NewRuntime(env, 64*1024, NULL);
        REQUIRE(runtime != nullptr);
        std::cerr << "Test: loading module into runtime" << std::endl;
        r = m3_LoadModule(runtime, module);
        REQUIRE(r == m3Err_none);
        std::cerr << "Test: module loaded" << std::endl;

        auto tok = hb.RegisterVoidString("env", "host_log", [&](const std::string& s){ captured = s; });
        REQUIRE(tok.valid());
        std::cerr << "Test: registered host callback" << std::endl;

        // Call exported 'run' function
        IM3Function f = nullptr;
        M3Result r2 = m3_FindFunction(&f, runtime, "run");
        REQUIRE(r2 == m3Err_none);
        std::cerr << "Test: calling run" << std::endl;
        r2 = m3_CallArgv(f, 0, nullptr);
        std::cerr << "Test: call returned: " << r2 << std::endl;
        REQUIRE(r2 == m3Err_none);

        REQUIRE(captured == "hello");
    }

    // Cleanup
    if (runtime) m3_FreeRuntime(runtime);
    m3_FreeModule(module);
    m3_FreeEnvironment(env);
}

    TEST_CASE("Wasm HostBindings: host_log rejects over-long strings", "[wasm][host][limits]") {
        // Build the same bytes as the previous test (file fallback -> embedded hex)
        std::filesystem::path binPath = std::filesystem::path(PROJECT_SOURCE_DIR) / "Tests" / "data" / "wasm" / "host_bindings" / "host_log.wasm";
        std::vector<uint8_t> bytes;
        if (std::filesystem::exists(binPath)) {
            std::ifstream ifs(binPath, std::ios::binary);
            std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            bytes.assign(s.begin(), s.end());
        } else {
            bytes = HexToBytes(kHostLogWasmHex);
        }

        IM3Environment env2 = m3_NewEnvironment();
        REQUIRE(env2 != nullptr);
        IM3Module module2 = nullptr;
        M3Result r = m3_ParseModule(env2, &module2, bytes.data(), bytes.size());
        REQUIRE(r == m3Err_none);
        IM3Runtime runtime2 = m3_NewRuntime(env2, 64*1024, NULL);
        REQUIRE(runtime2 != nullptr);
        r = m3_LoadModule(runtime2, module2);
        REQUIRE(r == m3Err_none);

        Genesis::Engine::HostBindings hb2(module2);
        hb2.set_max_string_length(3); // reject strings longer than 3
        std::string captured2;
        auto tok2 = hb2.RegisterVoidString("env", "host_log", [&](const std::string& s){ captured2 = s; });
        REQUIRE(tok2.valid());

        IM3Function f2 = nullptr;
        M3Result r2 = m3_FindFunction(&f2, runtime2, "run");
        REQUIRE(r2 == m3Err_none);
        r2 = m3_CallArgv(f2, 0, nullptr);
        // Expect a trap because string length (5) > 3
        REQUIRE(r2 != m3Err_none);
        REQUIRE(captured2.empty());
        // Destroy the registration token explicitly before freeing module/runtime to
        // avoid potential ordering issues where destructor might call into wasm3.
        tok2 = {};
        std::cerr << "Test: post-assert reached; token cleared; about to cleanup" << std::endl;

        if (runtime2) { std::cerr << "Test: about to free runtime2" << std::endl; m3_FreeRuntime(runtime2); std::cerr << "Test: freed runtime2" << std::endl; }
        std::cerr << "Test: about to free module2" << std::endl; m3_FreeModule(module2); std::cerr << "Test: freed module2" << std::endl;
        std::cerr << "Test: about to free env2" << std::endl; m3_FreeEnvironment(env2); std::cerr << "Test: freed env2" << std::endl;
    }

    TEST_CASE("Wasm HostBindings: token destruction doesn't crash", "[wasm][host][uaf]") {
        // Reuse the same module bytes as above
        std::filesystem::path binPath = std::filesystem::path(PROJECT_SOURCE_DIR) / "Tests" / "data" / "wasm" / "host_bindings" / "host_log.wasm";
        std::vector<uint8_t> bytes;
        if (std::filesystem::exists(binPath)) {
            std::ifstream ifs(binPath, std::ios::binary);
            std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            bytes.assign(s.begin(), s.end());
        } else {
            bytes = HexToBytes(kHostLogWasmHex);
        }

        IM3Environment env3 = m3_NewEnvironment();
        REQUIRE(env3 != nullptr);
        IM3Module module3 = nullptr;
        M3Result r = m3_ParseModule(env3, &module3, bytes.data(), bytes.size());
        REQUIRE(r == m3Err_none);
        IM3Runtime runtime3 = m3_NewRuntime(env3, 64*1024, NULL);
        REQUIRE(runtime3 != nullptr);
        r = m3_LoadModule(runtime3, module3);
        REQUIRE(r == m3Err_none);

        Genesis::Engine::HostBindings hb3(module3);
        std::string captured3;
        {
            auto tok = hb3.RegisterVoidString("env", "host_log", [&](const std::string& s){ captured3 = s; });
            REQUIRE(tok.valid());
            // Destroy the token explicitly to simulate unregistering while module is still loaded
            tok = {};

            IM3Function f3 = nullptr;
            M3Result r3 = m3_FindFunction(&f3, runtime3, "run");
            REQUIRE(r3 == m3Err_none);
            r3 = m3_CallArgv(f3, 0, nullptr);
            // We expect the call to either trap or otherwise not succeed but crucially not crash
            REQUIRE(r3 != m3Err_none);
        }

        if (runtime3) m3_FreeRuntime(runtime3);
        m3_FreeModule(module3);
        m3_FreeEnvironment(env3);
    }

    TEST_CASE("WasmRuntime: blocking host callback times out and module is quarantined", "[wasm][runtime][timeout]") {
        REQUIRE(Genesis::Engine::WasmRuntime::Init() == true);
        Genesis::Engine::ResourceLimits rl;
        rl.execution_time_ms = 50;
        Genesis::Engine::WasmRuntime::SetDefaultResourceLimits(rl);

        // Register a global host function that sleeps (defined above)
        auto token = Genesis::Engine::WasmRuntime::RegisterHostFunction("env", "host_log", "v(ii)", (M3RawCall)test_host_sleep);

        // Build module bytes as in other tests
        std::filesystem::path binPath = std::filesystem::path(PROJECT_SOURCE_DIR) / "Tests" / "data" / "wasm" / "host_bindings" / "host_log.wasm";
        std::vector<uint8_t> bytes;
        if (std::filesystem::exists(binPath)) {
            std::ifstream ifs(binPath, std::ios::binary);
            std::string s((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
            bytes.assign(s.begin(), s.end());
        } else {
            bytes = HexToBytes(kHostLogWasmHex);
        }

        REQUIRE(Genesis::Engine::WasmRuntime::LoadModuleFromBytes("sleep_test", bytes) == true);
        bool ok = Genesis::Engine::WasmRuntime::CallExportedWithTimeout("sleep_test", "run", {}, 50);
        REQUIRE(ok == false);
        bool ok2 = Genesis::Engine::WasmRuntime::CallExported("sleep_test", "run");
        REQUIRE(ok2 == false);

        Genesis::Engine::WasmRuntime::Shutdown();
    }

#else
TEST_CASE("Wasm HostBindings: wasm3 not available", "[wasm][host]") {
    SUCCEED("wasm3 not available; skipping host bindings test");
}
#endif
