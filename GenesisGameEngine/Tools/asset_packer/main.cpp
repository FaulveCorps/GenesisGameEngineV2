#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
#include <string>

namespace fs = std::filesystem;

struct FileEntry {
    std::string path; // relative path
    uint64_t size;
};

bool pack(const fs::path& inputDir, const fs::path& outFile) {
    std::vector<FileEntry> entries;
    std::vector<std::string> data;

    for (auto& p : fs::recursive_directory_iterator(inputDir)) {
        if (!p.is_regular_file()) continue;
        fs::path rel = fs::relative(p.path(), inputDir);
        FileEntry e;
        e.path = rel.generic_string();
        e.size = (uint64_t)fs::file_size(p.path());
        entries.push_back(e);
        std::ifstream in(p.path(), std::ios::binary);
        std::string buf;
        buf.resize((size_t)e.size);
        in.read(buf.data(), (std::streamsize)e.size);
        data.push_back(std::move(buf));
    }

    std::ofstream out(outFile, std::ios::binary);
    if (!out) {
        std::cerr << "Failed to open output file: " << outFile << std::endl;
        return false;
    }

    // Write magic
    out.write("GPK1", 4);
    uint32_t count = (uint32_t)entries.size();
    out.write(reinterpret_cast<const char*>(&count), sizeof(count));

    for (size_t i = 0; i < entries.size(); ++i) {
        const auto &e = entries[i];
        uint32_t nameLen = (uint32_t)e.path.size();
        out.write(reinterpret_cast<const char*>(&nameLen), sizeof(nameLen));
        out.write(e.path.c_str(), nameLen);
        out.write(reinterpret_cast<const char*>(&e.size), sizeof(e.size));
        out.write(data[i].data(), (std::streamsize)e.size);
    }

    std::cout << "Packed " << count << " files into " << outFile << std::endl;
    return true;
}

bool unpack(const fs::path& archive, const fs::path& outDir) {
    std::ifstream in(archive, std::ios::binary);
    if (!in) {
        std::cerr << "Failed to open archive: " << archive << std::endl;
        return false;
    }

    char magic[4];
    in.read(magic, 4);
    if (in.gcount() != 4 || std::string(magic,4) != "GPK1") {
        std::cerr << "Invalid archive format" << std::endl;
        return false;
    }

    uint32_t count;
    in.read(reinterpret_cast<char*>(&count), sizeof(count));

    for (uint32_t i = 0; i < count; ++i) {
        uint32_t nameLen;
        in.read(reinterpret_cast<char*>(&nameLen), sizeof(nameLen));
        std::string name(nameLen, '\0');
        in.read(name.data(), nameLen);
        uint64_t size;
        in.read(reinterpret_cast<char*>(&size), sizeof(size));
        std::string buf((size_t)size, '\0');
        in.read(buf.data(), (std::streamsize)size);

        fs::path outPath = outDir / name;
        fs::create_directories(outPath.parent_path());
        std::ofstream out(outPath, std::ios::binary);
        out.write(buf.data(), (std::streamsize)size);
    }

    std::cout << "Unpacked " << count << " files to " << outDir << std::endl;
    return true;
}

int main(int argc, char** argv) {
    if (argc < 4) {
        std::cout << "Usage:\n  pack <input_dir> <output_file>\n  unpack <archive> <output_dir>" << std::endl;
        return 1;
    }

    std::string cmd = argv[1];
    if (cmd == "pack") {
        return pack(argv[2], argv[3]) ? 0 : 1;
    } else if (cmd == "unpack") {
        return unpack(argv[2], argv[3]) ? 0 : 1;
    }

    std::cout << "Unknown command" << std::endl;
    return 1;
}
