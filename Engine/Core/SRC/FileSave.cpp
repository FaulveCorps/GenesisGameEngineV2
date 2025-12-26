#include "engine/ISave.h"
#include "engine/SubsystemRegistry.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace Genesis::Engine {

class FileSave : public ISave {
public:
    FileSave() {}

    bool Init() override {
        const char* env = std::getenv("GENESIS_SAVE_DIR");
        if (env && *env) m_saveDir = std::filesystem::path(env);
        else m_saveDir = std::filesystem::path("saves");

        try {
            std::filesystem::create_directories(m_saveDir);
        } catch (const std::exception& e) {
            std::cerr << "FileSave: failed to create save directory '" << m_saveDir.string() << "': " << e.what() << std::endl;
            return false;
        }
        std::cout << "FileSave: Init (dir='" << m_saveDir.string() << "')" << std::endl;
        return true;
    }

    void Update(double /*dt*/) override { /* no-op for now */ }

    void Shutdown() override {
        // nothing to do
    }

    std::string Name() const override { return "file"; }

    bool Save(const std::string& slot, const std::string& data) override {
        if (slot.empty()) return false;
        std::string safe = Sanitize(slot);
        auto finalPath = m_saveDir / (safe + ".sav");
        auto tmpPath = m_saveDir / (safe + ".tmp");

        try {
            std::ofstream ofs(tmpPath, std::ios::binary | std::ios::trunc);
            if (!ofs) return false;
            ofs.write(data.data(), static_cast<std::streamsize>(data.size()));
            ofs.close();
            // Atomic-ish move
            if (std::filesystem::exists(finalPath)) std::filesystem::remove(finalPath);
            std::filesystem::rename(tmpPath, finalPath);
            return true;
        } catch (const std::exception& e) {
            std::cerr << "FileSave: Save failed for slot='" << slot << "': " << e.what() << std::endl;
            try { if (std::filesystem::exists(tmpPath)) std::filesystem::remove(tmpPath); } catch(...) {}
            return false;
        }
    }

    bool Load(const std::string& slot, std::string& out) override {
        if (slot.empty()) return false;
        std::string safe = Sanitize(slot);
        auto finalPath = m_saveDir / (safe + ".sav");
        if (!std::filesystem::exists(finalPath)) return false;
        try {
            std::ifstream ifs(finalPath, std::ios::binary);
            if (!ifs) return false;
            std::ostringstream ss;
            ss << ifs.rdbuf();
            out = ss.str();
            return true;
        } catch (const std::exception& e) {
            std::cerr << "FileSave: Load failed for slot='" << slot << "': " << e.what() << std::endl;
            return false;
        }
    }

    bool Delete(const std::string& slot) override {
        if (slot.empty()) return false;
        std::string safe = Sanitize(slot);
        auto finalPath = m_saveDir / (safe + ".sav");
        try {
            if (std::filesystem::exists(finalPath)) {
                std::filesystem::remove(finalPath);
            }
            return true;
        } catch (...) {
            return false;
        }
    }

    std::vector<std::string> ListSlots() const override {
        std::vector<std::string> res;
        try {
            if (!std::filesystem::exists(m_saveDir)) return res;
            for (auto &p : std::filesystem::directory_iterator(m_saveDir)) {
                if (!p.is_regular_file()) continue;
                auto ext = p.path().extension();
                if (ext == ".sav") {
                    auto name = p.path().stem().string();
                    res.push_back(name);
                }
            }
        } catch (...) {}
        return res;
    }

private:
    std::filesystem::path m_saveDir;

    static std::string Sanitize(const std::string& s) {
        std::string out;
        out.reserve(s.size());
        for (char c : s) {
            if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-') out.push_back(c);
            else out.push_back('_');
        }
        return out;
    }
};

static bool register_file_save = []() {
    SubsystemRegistry::Instance().RegisterFactory("Save", "file", []() {
        return std::make_unique<FileSave>();
    });
    return true;
}();

void RegisterFileSaveFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Save", "file", []() {
        return std::make_unique<FileSave>();
    });
}

} // namespace Genesis::Engine
