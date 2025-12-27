#include <iostream>
#include <fstream>
#include <algorithm>
#include "engine/WasmRuntime.h"
#include "engine/Engine.h"

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

int main() {
    std::cerr << "drill: start" << std::endl;
    Genesis::Engine::WasmRuntime::Init();
    std::string noop = "0061736d0100000001040160000030020100070c01086d6f645f696e69740000";
    auto bytes = HexToBytes(noop);
    std::cerr << "drill: bytes size=" << bytes.size() << std::endl;
    bool ok = Genesis::Engine::WasmRuntime::LoadModuleFromBytes("drill_noop", bytes);
    std::cerr << "drill: LoadModuleFromBytes returned " << ok << std::endl;
    auto loaded = Genesis::Engine::WasmRuntime::LoadedModules();
    std::cerr << "drill: loaded count=" << loaded.size() << std::endl;
    for (auto &s : loaded) std::cerr << "drill: module='" << s << "'" << std::endl;

    // Compare mod.wasm binary vs mod.wasm.hex conversion
    std::filesystem::path srcDir = std::filesystem::path(PROJECT_SOURCE_DIR);
    std::filesystem::path hexPath = srcDir / "GameProjects" / "SampleGame" / "mods" / "sample_mod" / "mod.wasm.hex";
    std::filesystem::path wasmPath = srcDir / "GameProjects" / "SampleGame" / "mods" / "sample_mod" / "mod.wasm";
    if (std::filesystem::exists(hexPath) && std::filesystem::exists(wasmPath)) {
        std::ifstream ifs(hexPath);
        std::string hex((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string cleaned;
        for (char c : hex) if (!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);
        auto hexBytes = HexToBytes(cleaned);
        std::ifstream ifs2(wasmPath, std::ios::binary);
        std::string binStr((std::istreambuf_iterator<char>(ifs2)), std::istreambuf_iterator<char>());
        std::vector<uint8_t> bin(binStr.begin(), binStr.end());
        std::cerr << "drill: hexBytes size=" << hexBytes.size() << " bin size=" << bin.size() << std::endl;
        size_t minSz = std::min(hexBytes.size(), bin.size());
        for (size_t i = 0;i < minSz && i < 64;i++) {
            std::cerr << std::hex << i << ": " << (int)hexBytes[i] << " / " << (int)bin[i] << std::dec << std::endl;
        }
        if (hexBytes.size() == bin.size() && std::equal(hexBytes.begin(), hexBytes.end(), bin.begin())) std::cerr << "drill: hex file matches binary" << std::endl;
        else std::cerr << "drill: hex file DOES NOT match binary" << std::endl;
    } else {
        std::cerr << "drill: sample mod files not present" << std::endl;
    }

    std::cerr << "drill: now shutdown" << std::endl;
    Genesis::Engine::WasmRuntime::Shutdown();
    std::cerr << "drill: done" << std::endl;
    return 0;
}
