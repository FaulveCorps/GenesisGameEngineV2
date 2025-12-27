#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <cstdlib>

namespace fs = std::filesystem;

int compileWithGlslang(const fs::path& src, const fs::path& out) {
    std::string cmd = "glslangValidator -V \"" + src.string() + "\" -o \"" + out.string() + "\"";
    int rc = std::system(cmd.c_str());
    return rc;
}

int main(int argc, char** argv) {
    if (argc < 3) {
        std::cout << "Usage: shader_compiler <input_shader> <output_spv>" << std::endl;
        return 1;
    }
    fs::path src = argv[1];
    fs::path out = argv[2];

    if (!fs::exists(src)) {
        std::cerr << "Source file does not exist: " << src << std::endl;
        return 1;
    }

    // Try to use glslangValidator if present
    int rc = compileWithGlslang(src, out);
    if (rc == 0) {
        std::cout << "Compiled shader to " << out << std::endl;
        return 0;
    }

    // Fallback: copy source to output (with a warning)
    std::cerr << "glslangValidator not available or failed; copying source to output as fallback" << std::endl;
    std::ifstream in(src, std::ios::binary);
    std::ofstream o(out, std::ios::binary);
    o << in.rdbuf();
    std::cout << "Copied shader to " << out << std::endl;
    return 0;
}
