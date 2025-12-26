#include <iostream>
#include <fstream>
#include <vector>
#include <filesystem>
#include "engine/WasmRuntime.h"
#include "engine/Engine.h"

int main() {
    if (!Genesis::Engine::Init()) {
        std::cerr << "Engine::Init failed" << std::endl;
        return 2;
    }

    namespace fs = std::filesystem;
    fs::path wasmPath = fs::path(PROJECT_SOURCE_DIR) / "GameProjects" / "SampleGame" / "mods" / "sample_mod" / "mod.wasm";

    if (!fs::exists(wasmPath)) {
        fs::path hexp = fs::path(PROJECT_SOURCE_DIR) / "GameProjects" / "SampleGame" / "mods" / "sample_mod" / "mod.wasm.hex";
        if (!fs::exists(hexp)) {
            std::cerr << "No mod.wasm or mod.wasm.hex present" << std::endl;
            return 3;
        }
        std::ifstream ifs(hexp);
        if (!ifs.good()) { std::cerr << "Failed to open hex file" << std::endl; return 4; }
        std::string hex((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());
        std::string cleaned;
        cleaned.reserve(hex.size());
        for (char c : hex) if (!std::isspace(static_cast<unsigned char>(c))) cleaned.push_back(c);
        std::vector<uint8_t> bytes;
        bytes.reserve(cleaned.size() / 2);
        auto cv = [](char c)->int { if (c >= '0'&&c <= '9') return c - '0'; if (c >= 'a'&&c <= 'f') return 10 + (c - 'a'); if (c >= 'A'&&c <= 'F') return 10 + (c - 'A'); return 0; };
        for (size_t i=0; i+1<cleaned.size(); i+=2) {
            uint8_t b = static_cast<uint8_t>((cv(cleaned[i]) << 4) | cv(cleaned[i+1]));
            bytes.push_back(b);
        }
        if (bytes.empty()) { std::cerr << "Hex file decoded to empty bytes" << std::endl; return 5; }
        if (!Genesis::Engine::WasmRuntime::LoadModuleFromBytes("sample_mod_smoke", bytes)) {
            std::cerr << "WasmRuntime::LoadModuleFromBytes failed" << std::endl; return 6;
        }
    } else {
        if (!Genesis::Engine::WasmRuntime::LoadModule(wasmPath)) {
            std::cerr << "WasmRuntime::LoadModule failed" << std::endl; return 7;
        }
    }

    auto modules = Genesis::Engine::WasmRuntime::LoadedModules();
    std::cout << "Loaded modules (" << modules.size() << "):" << std::endl;
    for (auto &m : modules) std::cout << " - " << m << std::endl;

    std::string basename = wasmPath.filename().string();
    if (Genesis::Engine::WasmRuntime::CallExported(basename, "mod_init")) {
        std::cout << "Called mod_init on " << basename << std::endl;
    } else {
        std::cout << "mod_init not present or failed on " << basename << std::endl;
    }

    return 0;
}
