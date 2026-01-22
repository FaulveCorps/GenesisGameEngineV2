#include "Engine/AssetDatabase.h"
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <algorithm>
#include <random>
#include <chrono>

namespace Genesis::Engine {

static std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static std::string NormalizePath(const std::filesystem::path& path, const std::filesystem::path& root) {
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::weakly_canonical(path, ec);
    if (ec) abs = path;
    std::filesystem::path rel = std::filesystem::relative(abs, root, ec);
    if (!ec) {
        std::string relStr = rel.generic_string();
        if (!relStr.empty() && relStr.rfind("..", 0) != 0) return relStr;
    }
    return abs.generic_string();
}

static uint64_t FileTimeToUnix(const std::filesystem::file_time_type& ftime) {
    using namespace std::chrono;
    auto sctp = time_point_cast<seconds>(ftime - std::filesystem::file_time_type::clock::now() + system_clock::now());
    return (uint64_t)duration_cast<seconds>(sctp.time_since_epoch()).count();
}

static std::string GenerateGuid() {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> dist(0, 0xFFFFFFFFu);

    uint32_t a = dist(gen);
    uint32_t b = dist(gen);
    uint32_t c = dist(gen);
    uint32_t d = dist(gen);

    char buf[64] = {};
    std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%04x%08x",
                  a,
                  (b >> 16) & 0xFFFF,
                  b & 0xFFFF,
                  (c >> 16) & 0xFFFF,
                  c & 0xFFFF,
                  d);
    return std::string(buf);
}

static void AddDependency(std::vector<std::string>& deps, const std::string& path) {
    if (path.empty()) return;
    if (std::find(deps.begin(), deps.end(), path) == deps.end()) deps.push_back(path);
}

static std::vector<std::string> ExtractQuotedUris(const std::string& content) {
    std::vector<std::string> out;
    size_t pos = 0;
    while ((pos = content.find("\"uri\"", pos)) != std::string::npos) {
        size_t colon = content.find(':', pos);
        if (colon == std::string::npos) break;
        size_t firstQuote = content.find('"', colon + 1);
        if (firstQuote == std::string::npos) break;
        size_t secondQuote = content.find('"', firstQuote + 1);
        if (secondQuote == std::string::npos) break;
        std::string uri = content.substr(firstQuote + 1, secondQuote - firstQuote - 1);
        out.push_back(uri);
        pos = secondQuote + 1;
    }
    return out;
}

static void CollectDependenciesForObj(const std::filesystem::path& objPath, std::vector<std::string>& deps) {
    std::ifstream file(objPath);
    if (!file.is_open()) return;

    std::filesystem::path baseDir = objPath.parent_path();
    std::vector<std::filesystem::path> mtls;

    std::string line;
    while (std::getline(file, line)) {
        std::stringstream ss(line);
        std::string token;
        ss >> token;
        if (token == "mtllib") {
            std::string mtl;
            ss >> mtl;
            if (!mtl.empty()) {
                mtls.push_back(baseDir / mtl);
            }
        }
    }

    for (const auto& mtlPath : mtls) {
        AddDependency(deps, mtlPath.generic_string());
        std::ifstream mtl(mtlPath);
        if (!mtl.is_open()) continue;
        std::string line2;
        while (std::getline(mtl, line2)) {
            std::stringstream ss(line2);
            std::string token;
            ss >> token;
            if (token == "map_Kd" || token == "map_Ks" || token == "map_Ka" || token == "map_Bump" || token == "bump" || token == "disp" || token == "norm") {
                std::string tex;
                std::string part;
                while (ss >> part) {
                    tex = part;
                }
                if (!tex.empty()) {
                    AddDependency(deps, (baseDir / tex).generic_string());
                }
            }
        }
    }
}

static void CollectDependenciesForSceneFile(const std::filesystem::path& scenePath, std::vector<std::string>& deps) {
    std::ifstream file(scenePath);
    if (!file.is_open()) return;

    std::filesystem::path baseDir = scenePath.parent_path();
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string token;
        ss >> token;
        if (token == "MODEL") {
            std::string path;
            ss >> path;
            if (!path.empty()) AddDependency(deps, (baseDir / path).generic_string());
        } else if (token == "AUDIO") {
            std::string path;
            ss >> path;
            if (!path.empty() && path != "NONE") AddDependency(deps, (baseDir / path).generic_string());
        } else if (token == "MAT_OVERRIDE") {
            int idx;
            float r, g, b, a, m, rough;
            std::string baseTex, normTex;
            ss >> idx >> r >> g >> b >> a >> m >> rough >> baseTex >> normTex;
            if (!baseTex.empty() && baseTex != "NONE") AddDependency(deps, (baseDir / baseTex).generic_string());
            if (!normTex.empty() && normTex != "NONE") AddDependency(deps, (baseDir / normTex).generic_string());
        } else if (token == "PREFAB_INSTANCE") {
            std::string path;
            ss >> path;
            if (!path.empty() && path != "NONE") AddDependency(deps, (baseDir / path).generic_string());
        }
    }
}

static std::vector<std::string> CollectDependencies(const std::filesystem::path& assetPath) {
    std::vector<std::string> deps;
    std::string ext = ToLowerCopy(assetPath.extension().string());

    if (ext == ".obj") {
        CollectDependenciesForObj(assetPath, deps);
    } else if (ext == ".gltf") {
        std::ifstream file(assetPath);
        if (file.is_open()) {
            std::stringstream buffer;
            buffer << file.rdbuf();
            auto uris = ExtractQuotedUris(buffer.str());
            std::filesystem::path baseDir = assetPath.parent_path();
            for (const auto& uri : uris) {
                if (!uri.empty()) {
                    AddDependency(deps, (baseDir / uri).generic_string());
                }
            }
        }
    } else if (ext == ".scene" || ext == ".prefab") {
        CollectDependenciesForSceneFile(assetPath, deps);
    }

    return deps;
}

static std::string InferImporter(const std::filesystem::path& assetPath) {
    std::string ext = ToLowerCopy(assetPath.extension().string());
    if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") return "texture";
    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") return "model";
    if (ext == ".wav" || ext == ".mp3" || ext == ".ogg") return "audio";
    if (ext == ".scene") return "scene";
    if (ext == ".prefab") return "prefab";
    if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".hlsl" || ext == ".spv") return "shader";
    if (ext == ".ttf" || ext == ".otf") return "font";
    if (ext == ".lua" || ext == ".cs" || ext == ".js") return "script";
    return "unknown";
}

std::filesystem::path AssetDatabase::GetMetaPath(const std::filesystem::path& assetPath) {
    return std::filesystem::path(assetPath.string() + ".meta");
}

bool AssetDatabase::IsMetaFile(const std::filesystem::path& path) {
    return path.extension() == ".meta";
}

bool AssetDatabase::LoadMeta(const std::filesystem::path& assetPath, AssetMeta& outMeta) {
    std::ifstream file(GetMetaPath(assetPath));
    if (!file.is_open()) return false;

    std::string line;
    outMeta.dependencies.clear();
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string value = line.substr(eq + 1);

        if (key == "guid") outMeta.guid = value;
        else if (key == "importer") outMeta.importer = value;
        else if (key == "source_timestamp") outMeta.sourceTimestamp = std::stoull(value);
        else if (key == "dependency") outMeta.dependencies.push_back(value);
    }

    return true;
}

bool AssetDatabase::SaveMeta(const std::filesystem::path& assetPath, const AssetMeta& meta) {
    std::ofstream file(GetMetaPath(assetPath), std::ios::trunc);
    if (!file.is_open()) return false;

    file << "# Genesis Asset Meta\n";
    file << "guid=" << meta.guid << "\n";
    file << "importer=" << meta.importer << "\n";
    file << "source_timestamp=" << meta.sourceTimestamp << "\n";
    for (const auto& dep : meta.dependencies) {
        file << "dependency=" << dep << "\n";
    }
    return true;
}

AssetMeta AssetDatabase::EnsureMeta(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot) {
    AssetMeta meta;
    if (LoadMeta(assetPath, meta)) return meta;

    Reimport(assetPath, projectRoot, &meta);
    return meta;
}

bool AssetDatabase::Reimport(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot, AssetMeta* outMeta) {
    if (!std::filesystem::exists(assetPath)) return false;

    AssetMeta meta;
    if (!LoadMeta(assetPath, meta)) {
        meta.guid = GenerateGuid();
    }

    meta.importer = InferImporter(assetPath);
    meta.dependencies.clear();

    std::vector<std::string> deps = CollectDependencies(assetPath);
    for (const auto& dep : deps) {
        meta.dependencies.push_back(NormalizePath(dep, projectRoot));
    }

    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(assetPath, ec);
    if (!ec) meta.sourceTimestamp = FileTimeToUnix(ftime);

    if (!SaveMeta(assetPath, meta)) return false;
    if (outMeta) *outMeta = meta;
    return true;
}

bool AssetDatabase::GetSourceTimestamp(const std::filesystem::path& assetPath, uint64_t& outTimestamp) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(assetPath, ec);
    if (ec) return false;
    outTimestamp = FileTimeToUnix(ftime);
    return true;
}

} // namespace Genesis::Engine
