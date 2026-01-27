#include "Engine/AssetDatabase.h"
#include <fstream>
#include <sstream>
#include <unordered_set>
#include <unordered_map>
#include <algorithm>
#include <functional>
#include <cctype>
#include <cstdint>
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

static std::filesystem::path ResolveDependencyPath(const std::string& dep, const std::filesystem::path& projectRoot) {
    std::filesystem::path p(dep);
    if (p.is_relative()) {
        return projectRoot / p;
    }
    return p;
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

static void UpsertSetting(std::vector<AssetMeta::ImportSetting>& settings, const std::string& key, const std::string& value) {
    for (auto& setting : settings) {
        if (setting.key == key) {
            setting.value = value;
            return;
        }
    }
    settings.push_back(AssetMeta::ImportSetting{ key, value });
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

static std::string TrimLeftCopy(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return std::string();
    return s.substr(start);
}

static void CollectDependenciesForShaderFile(const std::filesystem::path& shaderPath, std::vector<std::string>& deps) {
    std::ifstream file(shaderPath);
    if (!file.is_open()) return;

    std::filesystem::path baseDir = shaderPath.parent_path();
    std::string line;
    while (std::getline(file, line)) {
        std::string trimmed = TrimLeftCopy(line);
        if (trimmed.rfind("#include", 0) != 0) continue;

        size_t quote = trimmed.find('"');
        size_t end = std::string::npos;
        if (quote != std::string::npos) {
            end = trimmed.find('"', quote + 1);
        } else {
            quote = trimmed.find('<');
            if (quote != std::string::npos) {
                end = trimmed.find('>', quote + 1);
            }
        }
        if (quote == std::string::npos || end == std::string::npos || end <= quote + 1) continue;

        std::string includePath = trimmed.substr(quote + 1, end - quote - 1);
        if (includePath.empty()) continue;
        AddDependency(deps, (baseDir / includePath).generic_string());
    }
}

static bool TryResolveGltfUri(const std::string& uri, const std::filesystem::path& baseDir, std::filesystem::path& outPath) {
    if (uri.empty()) return false;
    if (uri.rfind("data:", 0) == 0) return false;

    auto HexToInt = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return 10 + (c - 'a');
        if (c >= 'A' && c <= 'F') return 10 + (c - 'A');
        return -1;
    };
    auto DecodeUri = [&](const std::string& value) {
        std::string out;
        out.reserve(value.size());
        for (size_t i = 0; i < value.size(); ++i) {
            if (value[i] == '%' && i + 2 < value.size()) {
                int hi = HexToInt(value[i + 1]);
                int lo = HexToInt(value[i + 2]);
                if (hi >= 0 && lo >= 0) {
                    out.push_back(static_cast<char>((hi << 4) | lo));
                    i += 2;
                    continue;
                }
            }
            out.push_back(value[i]);
        }
        return out;
    };

    std::string decoded = DecodeUri(uri);

    if (decoded.rfind("file://", 0) == 0) {
        std::string pathPart = decoded.substr(7);
        if (!pathPart.empty() && pathPart[0] == '/' && pathPart.size() >= 3 && std::isalpha(static_cast<unsigned char>(pathPart[1])) && pathPart[2] == ':') {
            pathPart.erase(0, 1);
        }
        outPath = std::filesystem::path(pathPart);
        return true;
    }

    if (decoded.find("://") != std::string::npos) return false;

    outPath = baseDir / decoded;
    return true;
}

static bool ReadU32LE(std::ifstream& file, uint32_t& out) {
    uint8_t buf[4] = {};
    if (!file.read(reinterpret_cast<char*>(buf), sizeof(buf))) return false;
    out = (uint32_t)buf[0] | ((uint32_t)buf[1] << 8) | ((uint32_t)buf[2] << 16) | ((uint32_t)buf[3] << 24);
    return true;
}

static void CollectDependenciesForGlb(const std::filesystem::path& glbPath, std::vector<std::string>& deps) {
    std::ifstream file(glbPath, std::ios::binary);
    if (!file.is_open()) return;

    uint32_t magic = 0;
    uint32_t version = 0;
    uint32_t length = 0;
    if (!ReadU32LE(file, magic) || !ReadU32LE(file, version) || !ReadU32LE(file, length)) return;
    if (magic != 0x46546C67) return; // 'glTF'

    const uint32_t kChunkTypeJson = 0x4E4F534A; // 'JSON'
    std::filesystem::path baseDir = glbPath.parent_path();

    while (file && file.tellg() >= 0) {
        uint32_t chunkLength = 0;
        uint32_t chunkType = 0;
        if (!ReadU32LE(file, chunkLength) || !ReadU32LE(file, chunkType)) break;
        if (chunkLength == 0) break;

        if (chunkType == kChunkTypeJson) {
            std::string json(chunkLength, '\0');
            if (!file.read(json.data(), chunkLength)) break;
            auto uris = ExtractQuotedUris(json);
            for (const auto& uri : uris) {
                std::filesystem::path depPath;
                if (TryResolveGltfUri(uri, baseDir, depPath)) {
                    AddDependency(deps, depPath.generic_string());
                }
            }
        } else {
            file.seekg(chunkLength, std::ios::cur);
        }
    }
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
        } else if (token == "PREFAB_LINK") {
            std::string path;
            ss >> path;
            if (!path.empty() && path != "NONE") AddDependency(deps, (baseDir / path).generic_string());
        }
    }
}

static std::vector<std::string> CollectDependenciesForAsset(const std::filesystem::path& assetPath) {
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
                std::filesystem::path depPath;
                if (TryResolveGltfUri(uri, baseDir, depPath)) {
                    AddDependency(deps, depPath.generic_string());
                }
            }
        }
    } else if (ext == ".glb") {
        CollectDependenciesForGlb(assetPath, deps);
    } else if (ext == ".vert" || ext == ".frag" || ext == ".glsl" || ext == ".hlsl" || ext == ".spv") {
        CollectDependenciesForShaderFile(assetPath, deps);
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

static bool LooksLikeNormalMap(const std::string& nameLower) {
    return nameLower.find("normal") != std::string::npos
        || nameLower.find("norm") != std::string::npos
        || nameLower.find("nrm") != std::string::npos
        || nameLower.find("_n.") != std::string::npos;
}

static bool LooksLikeDataMap(const std::string& nameLower) {
    return nameLower.find("rough") != std::string::npos
        || nameLower.find("metal") != std::string::npos
        || nameLower.find("orm") != std::string::npos
        || nameLower.find("ao") != std::string::npos
        || nameLower.find("occlusion") != std::string::npos
        || nameLower.find("mask") != std::string::npos
        || nameLower.find("spec") != std::string::npos
        || nameLower.find("gloss") != std::string::npos
        || nameLower.find("height") != std::string::npos
        || nameLower.find("disp") != std::string::npos;
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
    outMeta.dependencyTimestamps.clear();
    outMeta.importSettings.clear();
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
        else if (key == "dependency_ts") {
            auto sep = value.rfind('|');
            if (sep != std::string::npos) {
                AssetMeta::DependencyStamp stamp;
                stamp.path = value.substr(0, sep);
                std::string tsStr = value.substr(sep + 1);
                if (!tsStr.empty()) {
                    stamp.timestamp = std::stoull(tsStr);
                }
                outMeta.dependencyTimestamps.push_back(std::move(stamp));
            }
        }
        else if (key == "setting") {
            auto sep = value.find('|');
            if (sep != std::string::npos) {
                std::string settingKey = value.substr(0, sep);
                std::string settingValue = value.substr(sep + 1);
                if (!settingKey.empty()) {
                    UpsertSetting(outMeta.importSettings, settingKey, settingValue);
                }
            }
        }
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
    for (const auto& dep : meta.dependencyTimestamps) {
        file << "dependency_ts=" << dep.path << "|" << dep.timestamp << "\n";
    }
    if (!meta.importSettings.empty()) {
        std::vector<AssetMeta::ImportSetting> settings = meta.importSettings;
        std::sort(settings.begin(), settings.end(), [](const auto& a, const auto& b) {
            if (a.key == b.key) return a.value < b.value;
            return a.key < b.key;
        });
        for (const auto& setting : settings) {
            file << "setting=" << setting.key << "|" << setting.value << "\n";
        }
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
    meta.dependencyTimestamps.clear();

    if (meta.importer == "texture") {
        const std::string filenameLower = ToLowerCopy(assetPath.filename().string());
        const bool looksNormal = LooksLikeNormalMap(filenameLower);
        const bool looksData = LooksLikeDataMap(filenameLower);

        std::string value;
        if (!GetImportSetting(meta, "srgb", value)) {
            const bool defaultSrgb = !(looksNormal || looksData);
            SetImportSetting(meta, "srgb", defaultSrgb ? "1" : "0");
        }
        if (!GetImportSetting(meta, "mipmaps", value)) {
            SetImportSetting(meta, "mipmaps", "1");
        }
        if (!GetImportSetting(meta, "normal_map", value)) {
            SetImportSetting(meta, "normal_map", looksNormal ? "1" : "0");
        }
        if (!GetImportSetting(meta, "wrap", value)) {
            SetImportSetting(meta, "wrap", "repeat");
        }
        if (!GetImportSetting(meta, "filter", value)) {
            SetImportSetting(meta, "filter", "linear");
        }
    }

    if (meta.importer == "model") {
        std::string value;
        if (!GetImportSetting(meta, "gen_normals", value)) {
            SetImportSetting(meta, "gen_normals", "1");
        }
        if (!GetImportSetting(meta, "flip_uvs", value)) {
            SetImportSetting(meta, "flip_uvs", "0");
        }
        if (!GetImportSetting(meta, "optimize_meshes", value)) {
            SetImportSetting(meta, "optimize_meshes", "1");
        }
        if (!GetImportSetting(meta, "pretransform_vertices", value)) {
            SetImportSetting(meta, "pretransform_vertices", "0");
        }
        if (!GetImportSetting(meta, "scale_factor", value)) {
            SetImportSetting(meta, "scale_factor", "1.0");
        }
    }

    std::vector<std::string> deps = CollectDependenciesForAsset(assetPath);
    for (const auto& dep : deps) {
        meta.dependencies.push_back(NormalizePath(dep, projectRoot));
    }

    std::sort(meta.dependencies.begin(), meta.dependencies.end());
    meta.dependencies.erase(std::unique(meta.dependencies.begin(), meta.dependencies.end()), meta.dependencies.end());

    for (const auto& dep : meta.dependencies) {
        uint64_t depTimestamp = 0;
        GetSourceTimestamp(ResolveDependencyPath(dep, projectRoot), depTimestamp);
        meta.dependencyTimestamps.push_back(AssetMeta::DependencyStamp{ dep, depTimestamp });
    }

    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(assetPath, ec);
    if (!ec) meta.sourceTimestamp = FileTimeToUnix(ftime);

    if (!SaveMeta(assetPath, meta)) return false;
    if (outMeta) *outMeta = meta;
    return true;
}

bool AssetDatabase::CollectDependencies(const std::filesystem::path& assetPath, const std::filesystem::path& projectRoot, std::vector<std::string>& outDependencies) {
    outDependencies.clear();
    if (!std::filesystem::exists(assetPath)) return false;

    std::vector<std::string> deps = CollectDependenciesForAsset(assetPath);
    for (const auto& dep : deps) {
        outDependencies.push_back(NormalizePath(dep, projectRoot));
    }

    std::sort(outDependencies.begin(), outDependencies.end());
    outDependencies.erase(std::unique(outDependencies.begin(), outDependencies.end()), outDependencies.end());
    return true;
}

std::vector<std::filesystem::path> AssetDatabase::BuildReimportOrder(const std::vector<std::filesystem::path>& roots, const std::filesystem::path& projectRoot) {
    std::vector<std::filesystem::path> order;
    if (roots.empty()) return order;

    std::unordered_set<std::string> visited;
    std::unordered_set<std::string> inStack;
    std::vector<std::string> rootKeys;
    rootKeys.reserve(roots.size());

    for (const auto& root : roots) {
        if (root.empty()) continue;
        rootKeys.push_back(NormalizePath(root, projectRoot));
    }

    std::sort(rootKeys.begin(), rootKeys.end());
    rootKeys.erase(std::unique(rootKeys.begin(), rootKeys.end()), rootKeys.end());

    std::function<void(const std::string&)> dfs = [&](const std::string& key) {
        if (visited.count(key) > 0) return;
        if (inStack.count(key) > 0) {
            std::cerr << "AssetDatabase: dependency cycle detected at " << key << std::endl;
            return;
        }
        inStack.insert(key);

        std::filesystem::path assetPath = ResolveDependencyPath(key, projectRoot);
        if (std::filesystem::exists(assetPath)) {
            std::vector<std::string> deps;
            if (AssetDatabase::CollectDependencies(assetPath, projectRoot, deps)) {
                std::sort(deps.begin(), deps.end());
                for (const auto& dep : deps) {
                    dfs(dep);
                }
            }
        }

        inStack.erase(key);
        visited.insert(key);
        order.push_back(ResolveDependencyPath(key, projectRoot));
    };

    for (const auto& key : rootKeys) {
        dfs(key);
    }

    return order;
}

bool AssetDatabase::GetSourceTimestamp(const std::filesystem::path& assetPath, uint64_t& outTimestamp) {
    std::error_code ec;
    auto ftime = std::filesystem::last_write_time(assetPath, ec);
    if (ec) return false;
    outTimestamp = FileTimeToUnix(ftime);
    return true;
}

bool AssetDatabase::DependenciesChanged(const AssetMeta& meta, const std::filesystem::path& projectRoot) {
    if (meta.dependencies.empty()) return false;

    std::unordered_map<std::string, uint64_t> stamps;
    stamps.reserve(meta.dependencyTimestamps.size());
    for (const auto& stamp : meta.dependencyTimestamps) {
        stamps[stamp.path] = stamp.timestamp;
    }

    for (const auto& dep : meta.dependencies) {
        uint64_t current = 0;
        if (!GetSourceTimestamp(ResolveDependencyPath(dep, projectRoot), current)) {
            return true;
        }

        auto it = stamps.find(dep);
        if (it == stamps.end() || it->second != current) {
            return true;
        }
    }

    return false;
}

void AssetDatabase::SetImportSetting(AssetMeta& meta, const std::string& key, const std::string& value) {
    if (key.empty()) return;
    UpsertSetting(meta.importSettings, key, value);
}

bool AssetDatabase::GetImportSetting(const AssetMeta& meta, const std::string& key, std::string& outValue) {
    if (key.empty()) return false;
    for (const auto& setting : meta.importSettings) {
        if (setting.key == key) {
            outValue = setting.value;
            return true;
        }
    }
    return false;
}

void AssetDatabase::RemoveImportSetting(AssetMeta& meta, const std::string& key) {
    if (key.empty()) return;
    meta.importSettings.erase(
        std::remove_if(meta.importSettings.begin(), meta.importSettings.end(),
                       [&](const AssetMeta::ImportSetting& s) { return s.key == key; }),
        meta.importSettings.end());
}

} // namespace Genesis::Engine
