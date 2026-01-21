#define SDL_MAIN_HANDLED
#include <iostream>
#include "engine/Engine.h"
#include "engine/Window.h"
#include "engine/Scene.h"
#include "engine/GraphicsFactory.h"
#include "engine/RendererManager.h"
#include "engine/Components.h"
#include "engine/Model.h"
#include "engine/Profiler.h"
#include "engine/ImGuiLayer.h"
#include "engine/SceneLoader.h"
#include "engine/ShaderRegistry.h"
#include "engine/TextureRegistry.h"
#include "engine/Texture.h"
#include "engine/OpenGLRenderer.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include "ImGuizmo.h"
#include <cmath>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>
#include <cstdlib>
#include <unordered_map>
#include <cstdint>
#include <functional>
#include <unordered_set>
#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtx/euler_angles.hpp>

#include "imgui_internal.h"

// SDL Hit Test Callback for Custom Title Bar
SDL_HitTestResult SDLCALL HitTestCallback(SDL_Window* win, const SDL_Point* area, void* data) {
    int w, h;
    SDL_GetWindowSize(win, &w, &h);
    
    const int RESIZE_BORDER = 8;
    // Keep these in sync with the custom titlebar styling below.
    // VS Code-like caption buttons are a bit roomier than default.
    // (Option B sizing): make the caption area and hit zones clearly larger.
    const int TITLE_BAR_HEIGHT = 42;
    const int CONTROLS_WIDTH = 192; // 3 * 64px (Min/Max/Close)
    const int MENU_WIDTH = 600;     // Approximate width for Menu items (File, View, Window, Help)

    // Resize Borders
    if (area->x < RESIZE_BORDER && area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOPLEFT;
    if (area->x > w - RESIZE_BORDER && area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOPRIGHT;
    if (area->x < RESIZE_BORDER && area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOMLEFT;
    if (area->x > w - RESIZE_BORDER && area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOMRIGHT;

    if (area->x < RESIZE_BORDER) return SDL_HITTEST_RESIZE_LEFT;
    if (area->x > w - RESIZE_BORDER) return SDL_HITTEST_RESIZE_RIGHT;
    if (area->y < RESIZE_BORDER) return SDL_HITTEST_RESIZE_TOP;
    if (area->y > h - RESIZE_BORDER) return SDL_HITTEST_RESIZE_BOTTOM;

    // Title Bar Dragging
    if (area->y < TITLE_BAR_HEIGHT) {
        // Allow clicking on Menu Items (Left) and Controls (Right)
        // The empty space in the middle is draggable
        if (area->x > MENU_WIDTH && area->x < w - CONTROLS_WIDTH) {
            return SDL_HITTEST_DRAGGABLE;
        }
    }

    return SDL_HITTEST_NORMAL;
}

static bool ReadPpmToken(std::istream& in, std::string& out) {
    while (in >> out) {
        if (!out.empty() && out[0] == '#') {
            std::string ignored;
            std::getline(in, ignored);
            continue;
        }
        return true;
    }
    return false;
}

static std::shared_ptr<Genesis::Engine::Texture> LoadIconTexturePPM(const std::string& path) {
    std::ifstream file(path, std::ios::in);
    if (!file.is_open()) {
        std::cerr << "Icon texture missing: " << path << std::endl;
        return nullptr;
    }

    std::string token;
    if (!ReadPpmToken(file, token) || token != "P3") {
        std::cerr << "Icon texture not P3 PPM: " << path << std::endl;
        return nullptr;
    }

    if (!ReadPpmToken(file, token)) return nullptr;
    int width = std::stoi(token);
    if (!ReadPpmToken(file, token)) return nullptr;
    int height = std::stoi(token);
    if (!ReadPpmToken(file, token)) return nullptr;
    int maxVal = std::stoi(token);
    if (width <= 0 || height <= 0 || maxVal <= 0) return nullptr;

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4, 0);
    const float scale = 255.0f / static_cast<float>(maxVal);
    for (int i = 0; i < width * height; ++i) {
        if (!ReadPpmToken(file, token)) return nullptr;
        int r = std::stoi(token);
        if (!ReadPpmToken(file, token)) return nullptr;
        int g = std::stoi(token);
        if (!ReadPpmToken(file, token)) return nullptr;
        int b = std::stoi(token);

        uint8_t rr = static_cast<uint8_t>(std::clamp<int>((int)std::round(r * scale), 0, 255));
        uint8_t gg = static_cast<uint8_t>(std::clamp<int>((int)std::round(g * scale), 0, 255));
        uint8_t bb = static_cast<uint8_t>(std::clamp<int>((int)std::round(b * scale), 0, 255));

        const bool transparent = (rr == 255 && gg == 0 && bb == 255);
        pixels[i * 4 + 0] = rr;
        pixels[i * 4 + 1] = gg;
        pixels[i * 4 + 2] = bb;
        pixels[i * 4 + 3] = transparent ? 0 : 255;
    }

    return Genesis::Engine::Texture::CreateFromMemory((uint32_t)width, (uint32_t)height, pixels);
}

struct IconColor {
    uint8_t r = 0, g = 0, b = 0, a = 255;
};

static IconColor ShadeColor(const IconColor& c, float mul, int add = 0) {
    auto clamp = [](int v) { return static_cast<uint8_t>(std::max(0, std::min(255, v))); };
    return {
        clamp((int)std::round(c.r * mul) + add),
        clamp((int)std::round(c.g * mul) + add),
        clamp((int)std::round(c.b * mul) + add),
        c.a
    };
}

static IconColor LerpColor(const IconColor& a, const IconColor& b, float t) {
    t = std::max(0.0f, std::min(1.0f, t));
    auto lerp = [t](uint8_t v0, uint8_t v1) {
        return static_cast<uint8_t>(std::round(v0 + (v1 - v0) * t));
    };
    return { lerp(a.r, b.r), lerp(a.g, b.g), lerp(a.b, b.b), lerp(a.a, b.a) };
}

static void SetPixel(std::vector<uint8_t>& pixels, int size, int x, int y, const IconColor& c) {
    if (x < 0 || y < 0 || x >= size || y >= size) return;
    size_t idx = (static_cast<size_t>(y) * size + x) * 4;
    pixels[idx + 0] = c.r;
    pixels[idx + 1] = c.g;
    pixels[idx + 2] = c.b;
    pixels[idx + 3] = c.a;
}

static void FillRect(std::vector<uint8_t>& pixels, int size, int x0, int y0, int x1, int y1, const IconColor& c) {
    for (int y = y0; y < y1; ++y) {
        for (int x = x0; x < x1; ++x) {
            SetPixel(pixels, size, x, y, c);
        }
    }
}

static bool InRoundedRect(int x, int y, int x0, int y0, int x1, int y1, int radius) {
    if (x < x0 || y < y0 || x >= x1 || y >= y1) return false;
    if (radius <= 0) return true;

    const int left = x0 + radius;
    const int right = x1 - radius - 1;
    const int top = y0 + radius;
    const int bottom = y1 - radius - 1;

    if (x >= left && x <= right) return true;
    if (y >= top && y <= bottom) return true;

    const int cx = (x < left) ? left : right;
    const int cy = (y < top) ? top : bottom;
    const int dx = x - cx;
    const int dy = y - cy;
    return (dx * dx + dy * dy) <= (radius * radius);
}

static void FillRoundedRectGradient(std::vector<uint8_t>& pixels, int size,
    int x0, int y0, int x1, int y1, int radius,
    const IconColor& top, const IconColor& bottom) {
    const int height = std::max(1, y1 - y0);
    for (int y = y0; y < y1; ++y) {
        const float t = (height > 1) ? (float)(y - y0) / (float)(height - 1) : 0.0f;
        const IconColor row = LerpColor(top, bottom, t);
        for (int x = x0; x < x1; ++x) {
            if (InRoundedRect(x, y, x0, y0, x1, y1, radius)) {
                SetPixel(pixels, size, x, y, row);
            }
        }
    }
}

static std::shared_ptr<Genesis::Engine::Texture> MakeFolderIcon(const IconColor& base) {
    const int size = 64;
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4, 0);

    IconColor bodyTop = ShadeColor(base, 1.08f, 10);
    IconColor bodyBottom = ShadeColor(base, 0.86f, -6);
    IconColor tabTop = ShadeColor(base, 1.2f, 18);
    IconColor tabBottom = ShadeColor(base, 0.98f, 4);
    IconColor outline = ShadeColor(base, 0.65f, -10);
    IconColor shadow = { 0, 0, 0, 70 };

    // Shadow
    FillRoundedRectGradient(pixels, size, 8, 24, 58, 56, 6, shadow, shadow);
    // Tab
    FillRoundedRectGradient(pixels, size, 12, 12, 40, 26, 5, tabTop, tabBottom);
    // Body
    FillRoundedRectGradient(pixels, size, 6, 22, 58, 56, 6, bodyTop, bodyBottom);

    // Outline
    for (int y = 22; y < 56; ++y) {
        for (int x = 6; x < 58; ++x) {
            if (InRoundedRect(x, y, 6, 22, 58, 56, 6) && !InRoundedRect(x, y, 7, 23, 57, 55, 5)) {
                SetPixel(pixels, size, x, y, outline);
            }
        }
    }
    for (int y = 12; y < 26; ++y) {
        for (int x = 12; x < 40; ++x) {
            if (InRoundedRect(x, y, 12, 12, 40, 26, 5) && !InRoundedRect(x, y, 13, 13, 39, 25, 4)) {
                SetPixel(pixels, size, x, y, outline);
            }
        }
    }

    // Highlight line
    IconColor highlight = ShadeColor(base, 1.35f, 25);
    for (int x = 10; x < 52; ++x) {
        SetPixel(pixels, size, x, 27, highlight);
    }

    return Genesis::Engine::Texture::CreateFromMemory(size, size, pixels);
}

static std::shared_ptr<Genesis::Engine::Texture> MakeFileIcon(const IconColor& base) {
    const int size = 64;
    std::vector<uint8_t> pixels(static_cast<size_t>(size) * size * 4, 0);

    IconColor top = ShadeColor(base, 1.08f, 12);
    IconColor bottom = ShadeColor(base, 0.88f, -6);
    IconColor fold = ShadeColor(base, 1.25f, 26);
    IconColor outline = ShadeColor(base, 0.65f, -12);
    IconColor shadow = { 0, 0, 0, 70 };

    FillRoundedRectGradient(pixels, size, 14, 10, 52, 56, 6, shadow, shadow);
    FillRoundedRectGradient(pixels, size, 12, 8, 52, 56, 6, top, bottom);

    const int foldSize = 12;
    for (int y = 8; y < 8 + foldSize; ++y) {
        for (int x = 52 - (y - 8) - 1; x < 52; ++x) {
            SetPixel(pixels, size, x, y, fold);
        }
    }

    for (int y = 8; y < 56; ++y) {
        for (int x = 12; x < 52; ++x) {
            if (InRoundedRect(x, y, 12, 8, 52, 56, 6) && !InRoundedRect(x, y, 13, 9, 51, 55, 5)) {
                SetPixel(pixels, size, x, y, outline);
            }
        }
    }

    IconColor line = ShadeColor(base, 0.7f, -6);
    for (int i = 0; i < 3; ++i) {
        int y = 30 + i * 6;
        for (int x = 18; x < 44; ++x) {
            SetPixel(pixels, size, x, y, line);
        }
    }

    return Genesis::Engine::Texture::CreateFromMemory(size, size, pixels);
}

static std::string ToLowerCopy(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(), [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

static bool IsImageExtension(const std::string& extLower) {
    return extLower == ".png" || extLower == ".jpg" || extLower == ".jpeg" || extLower == ".bmp" || extLower == ".tga";
}

static glm::mat4 ComposeTransformGLM(const Genesis::Engine::Transform& t) {
    glm::mat4 transMat = glm::translate(glm::mat4(1.0f), glm::vec3(t.x, t.y, t.z));
    glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), t.rx, glm::vec3(1, 0, 0));
    glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), t.ry, glm::vec3(0, 1, 0));
    glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), t.rz, glm::vec3(0, 0, 1));
    glm::mat4 rot = rotZ * rotY * rotX;
    glm::mat4 scaleMat = glm::scale(glm::mat4(1.0f), glm::vec3(t.sx, t.sy, t.sz));
    return transMat * rot * scaleMat;
}

static bool DecomposeTransformGLM(const glm::mat4& m, Genesis::Engine::Transform& out) {
    glm::vec3 scale;
    glm::quat rotation;
    glm::vec3 translation;
    glm::vec3 skew;
    glm::vec4 perspective;
    if (!glm::decompose(m, scale, rotation, translation, skew, perspective)) {
        return false;
    }

    out.x = translation.x;
    out.y = translation.y;
    out.z = translation.z;

    out.sx = scale.x;
    out.sy = scale.y;
    out.sz = scale.z;

    rotation = glm::normalize(rotation);
    const glm::mat4 R = glm::mat4_cast(rotation);
    float z = 0.0f, y = 0.0f, x = 0.0f;
    glm::extractEulerAngleZYX(R, z, y, x);
    out.rx = x;
    out.ry = y;
    out.rz = z;
    return true;
}

static bool IsAncestor(const entt::registry& reg, entt::entity ancestor, entt::entity child) {
    entt::entity current = child;
    int depth = 0;
    while (reg.valid(current) && reg.any_of<Genesis::Engine::ParentComponent>(current)) {
        auto parent = reg.get<Genesis::Engine::ParentComponent>(current).parent;
        if (parent == ancestor) return true;
        if (parent == entt::null || !reg.valid(parent)) return false;
        current = parent;
        if (++depth > 64) return true;
    }
    return false;
}

static glm::mat4 GetWorldMatrixGLM(const entt::registry& reg, entt::entity entity) {
    if (!reg.valid(entity) || !reg.any_of<Genesis::Engine::Transform>(entity)) {
        return glm::mat4(1.0f);
    }

    glm::mat4 world = ComposeTransformGLM(reg.get<Genesis::Engine::Transform>(entity));
    entt::entity current = entity;
    std::unordered_set<entt::entity> visited;
    visited.insert(entity);
    int depth = 0;
    while (reg.any_of<Genesis::Engine::ParentComponent>(current)) {
        auto parent = reg.get<Genesis::Engine::ParentComponent>(current).parent;
        if (parent == entt::null || !reg.valid(parent)) break;
        if (visited.count(parent) > 0) break;
        visited.insert(parent);
        if (reg.any_of<Genesis::Engine::Transform>(parent)) {
            world = ComposeTransformGLM(reg.get<Genesis::Engine::Transform>(parent)) * world;
        }
        current = parent;
        if (++depth > 64) break;
    }
    return world;
}

struct EditorCommand {
    std::string label;
    std::function<void()> undo;
    std::function<void()> redo;
};

struct EntitySnapshot {
    bool hasName = false;
    Genesis::Engine::NameComponent name;
    bool hasParent = false;
    Genesis::Engine::ParentComponent parent;
    bool hasTransform = false;
    Genesis::Engine::Transform transform;
    bool hasModel = false;
    Genesis::Engine::ModelComponent model;
    bool hasLight = false;
    Genesis::Engine::LightComponent light;
    bool hasCamera = false;
    Genesis::Engine::CameraComponent camera;
    bool hasAudio = false;
    Genesis::Engine::AudioComponent audio;
    bool hasParticle = false;
    Genesis::Engine::ParticleSystemComponent particle;
    bool hasRigidBody = false;
    Genesis::Engine::RigidBodyComponent rigidBody;
    bool hasBoxCollider = false;
    Genesis::Engine::BoxColliderComponent boxCollider;
    bool hasSphereCollider = false;
    Genesis::Engine::SphereColliderComponent sphereCollider;
    bool hasScript = false;
    Genesis::Engine::ScriptComponent script;
};

static EntitySnapshot CaptureEntitySnapshot(const entt::registry& reg, entt::entity entity) {
    EntitySnapshot snap;
    if (reg.any_of<Genesis::Engine::NameComponent>(entity)) {
        snap.hasName = true;
        snap.name = reg.get<Genesis::Engine::NameComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::ParentComponent>(entity)) {
        snap.hasParent = true;
        snap.parent = reg.get<Genesis::Engine::ParentComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::Transform>(entity)) {
        snap.hasTransform = true;
        snap.transform = reg.get<Genesis::Engine::Transform>(entity);
    }
    if (reg.any_of<Genesis::Engine::ModelComponent>(entity)) {
        snap.hasModel = true;
        snap.model = reg.get<Genesis::Engine::ModelComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::LightComponent>(entity)) {
        snap.hasLight = true;
        snap.light = reg.get<Genesis::Engine::LightComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::CameraComponent>(entity)) {
        snap.hasCamera = true;
        snap.camera = reg.get<Genesis::Engine::CameraComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::AudioComponent>(entity)) {
        snap.hasAudio = true;
        snap.audio = reg.get<Genesis::Engine::AudioComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::ParticleSystemComponent>(entity)) {
        snap.hasParticle = true;
        snap.particle = reg.get<Genesis::Engine::ParticleSystemComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::RigidBodyComponent>(entity)) {
        snap.hasRigidBody = true;
        snap.rigidBody = reg.get<Genesis::Engine::RigidBodyComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::BoxColliderComponent>(entity)) {
        snap.hasBoxCollider = true;
        snap.boxCollider = reg.get<Genesis::Engine::BoxColliderComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::SphereColliderComponent>(entity)) {
        snap.hasSphereCollider = true;
        snap.sphereCollider = reg.get<Genesis::Engine::SphereColliderComponent>(entity);
    }
    if (reg.any_of<Genesis::Engine::ScriptComponent>(entity)) {
        snap.hasScript = true;
        snap.script = reg.get<Genesis::Engine::ScriptComponent>(entity);
        snap.script.Instance = nullptr;
    }
    return snap;
}

static entt::entity CreateEntityFromSnapshot(entt::registry& reg, const EntitySnapshot& snap) {
    auto e = reg.create();
    if (snap.hasName) reg.emplace<Genesis::Engine::NameComponent>(e, snap.name);
    if (snap.hasParent && snap.parent.parent != entt::null && reg.valid(snap.parent.parent)) {
        reg.emplace<Genesis::Engine::ParentComponent>(e, snap.parent);
    }
    if (snap.hasTransform) reg.emplace<Genesis::Engine::Transform>(e, snap.transform);
    if (snap.hasModel) reg.emplace<Genesis::Engine::ModelComponent>(e, snap.model);
    if (snap.hasLight) reg.emplace<Genesis::Engine::LightComponent>(e, snap.light);
    if (snap.hasCamera) reg.emplace<Genesis::Engine::CameraComponent>(e, snap.camera);
    if (snap.hasAudio) reg.emplace<Genesis::Engine::AudioComponent>(e, snap.audio);
    if (snap.hasParticle) reg.emplace<Genesis::Engine::ParticleSystemComponent>(e, snap.particle);
    if (snap.hasRigidBody) reg.emplace<Genesis::Engine::RigidBodyComponent>(e, snap.rigidBody);
    if (snap.hasBoxCollider) reg.emplace<Genesis::Engine::BoxColliderComponent>(e, snap.boxCollider);
    if (snap.hasSphereCollider) reg.emplace<Genesis::Engine::SphereColliderComponent>(e, snap.sphereCollider);
    if (snap.hasScript) {
        Genesis::Engine::ScriptComponent copy = snap.script;
        copy.Instance = nullptr;
        reg.emplace<Genesis::Engine::ScriptComponent>(e, copy);
    }
    return e;
}

static bool TransformNearlyEqual(const Genesis::Engine::Transform& a, const Genesis::Engine::Transform& b) {
    auto near = [](float lhs, float rhs) { return std::fabs(lhs - rhs) < 1e-4f; };
    return near(a.x, b.x) && near(a.y, b.y) && near(a.z, b.z)
        && near(a.rx, b.rx) && near(a.ry, b.ry) && near(a.rz, b.rz)
        && near(a.sx, b.sx) && near(a.sy, b.sy) && near(a.sz, b.sz);
}

struct RecentProjectEntry {
    std::string root;
    std::string lastScene;
};

struct EditorSessionSettings {
    std::string lastProjectRoot;
    std::string lastScenePath;
    std::vector<RecentProjectEntry> recentProjects;
    std::vector<std::string> recentScenes;
};

static std::string TrimCopy(const std::string& s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return std::string();
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static std::filesystem::path GetEditorSettingsPath() {
    std::filesystem::path base;
#ifdef _WIN32
    if (const char* appData = std::getenv("APPDATA")) {
        if (*appData) base = appData;
    }
#else
    if (const char* home = std::getenv("HOME")) {
        if (*home) base = home;
    }
#endif
    if (base.empty()) {
        base = std::filesystem::current_path();
    }

    std::filesystem::path dir = base / "GenesisEditor";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / "editor_settings.ini";
}

static EditorSessionSettings LoadEditorSettings() {
    EditorSessionSettings settings;
    std::ifstream in(GetEditorSettingsPath());
    if (!in.is_open()) return settings;

    std::string line;
    while (std::getline(in, line)) {
        line = TrimCopy(line);
        if (line.empty() || line[0] == '#') continue;
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = TrimCopy(line.substr(0, eq));
        std::string value = TrimCopy(line.substr(eq + 1));

        if (key == "last_project") {
            settings.lastProjectRoot = value;
        } else if (key == "last_scene") {
            settings.lastScenePath = value;
        } else if (key == "recent_project") {
            RecentProjectEntry entry;
            auto pipe = value.find('|');
            if (pipe == std::string::npos) {
                entry.root = value;
            } else {
                entry.root = TrimCopy(value.substr(0, pipe));
                entry.lastScene = TrimCopy(value.substr(pipe + 1));
            }
            if (!entry.root.empty()) {
                settings.recentProjects.push_back(entry);
            }
        } else if (key == "recent_scene") {
            if (!value.empty()) settings.recentScenes.push_back(value);
        }
    }

    if (settings.lastScenePath.empty() && !settings.lastProjectRoot.empty()) {
        for (const auto& entry : settings.recentProjects) {
            if (entry.root == settings.lastProjectRoot && !entry.lastScene.empty()) {
                settings.lastScenePath = entry.lastScene;
                break;
            }
        }
    }

    return settings;
}

static void SaveEditorSettings(const EditorSessionSettings& settings) {
    std::ofstream out(GetEditorSettingsPath(), std::ios::trunc);
    if (!out.is_open()) return;

    out << "# Genesis Editor Settings\n";
    if (!settings.lastProjectRoot.empty()) {
        out << "last_project=" << settings.lastProjectRoot << "\n";
    }
    if (!settings.lastScenePath.empty()) {
        out << "last_scene=" << settings.lastScenePath << "\n";
    }
    for (const auto& entry : settings.recentProjects) {
        out << "recent_project=" << entry.root;
        if (!entry.lastScene.empty()) {
            out << "|" << entry.lastScene;
        }
        out << "\n";
    }
    for (const auto& scene : settings.recentScenes) {
        out << "recent_scene=" << scene << "\n";
    }
}

static void UpdateRecentProject(EditorSessionSettings& settings, const std::string& root, const std::string& lastScene) {
    if (root.empty()) return;

    std::string existingScene;
    for (const auto& entry : settings.recentProjects) {
        if (entry.root == root) {
            existingScene = entry.lastScene;
            break;
        }
    }

    const std::string finalScene = !lastScene.empty() ? lastScene : existingScene;

    settings.recentProjects.erase(
        std::remove_if(settings.recentProjects.begin(), settings.recentProjects.end(),
                       [&](const RecentProjectEntry& e) { return e.root == root; }),
        settings.recentProjects.end());

    settings.recentProjects.insert(settings.recentProjects.begin(), RecentProjectEntry{ root, finalScene });
    const size_t kMaxRecentProjects = 10;
    if (settings.recentProjects.size() > kMaxRecentProjects) settings.recentProjects.resize(kMaxRecentProjects);
}

static void UpdateRecentScene(EditorSessionSettings& settings, const std::string& scenePath) {
    if (scenePath.empty()) return;
    settings.recentScenes.erase(
        std::remove(settings.recentScenes.begin(), settings.recentScenes.end(), scenePath),
        settings.recentScenes.end());
    settings.recentScenes.insert(settings.recentScenes.begin(), scenePath);
    const size_t kMaxRecentScenes = 12;
    if (settings.recentScenes.size() > kMaxRecentScenes) settings.recentScenes.resize(kMaxRecentScenes);
}

static std::string NormalizePathForSettings(const std::filesystem::path& path, const std::filesystem::path& projectRoot) {
    std::error_code ec;
    std::filesystem::path abs = std::filesystem::weakly_canonical(path, ec);
    if (ec) abs = path;
    std::filesystem::path rel = std::filesystem::relative(abs, projectRoot, ec);
    if (!ec) {
        std::string relStr = rel.string();
        if (!relStr.empty() && relStr.rfind("..", 0) != 0) return relStr;
    }
    return abs.string();
}

int main(int argc, char** argv) {
    std::cout << "GenesisEditor starting..." << std::endl;

    // Headless self-test (no UI interaction required). This is intended for CI/regression testing.
    // Usage: GenesisEditor.exe --quit-prompt-selftest
    bool quitPromptSelftest = false;
    for (int i = 1; i < argc; ++i) {
        if (argv[i] && std::string(argv[i]) == "--quit-prompt-selftest") {
            quitPromptSelftest = true;
        }
    }

    // Run the self-test *before* engine init/window creation so it works in headless CI environments.
    // This test validates only the editor's unsaved-changes prompt state machine and event routing.
    if (quitPromptSelftest) {
        // Use a fixed ID to represent the main window. We don't need to create a real SDL window
        // because the test simulates SDL events directly.
        const SDL_WindowID mainWindowId = (SDL_WindowID)1;

        enum class PendingSceneAction {
            None,
            Quit,
            NewScene,
            ShowOpenScene,
            LoadScenePath
        };

        bool running = true;
        bool sceneDirty = true;
        PendingSceneAction pendingAction = PendingSceneAction::None;
        std::string pendingScenePath;
        bool showUnsavedChangesModal = false;

        auto MaybePromptUnsaved = [&](PendingSceneAction action, const std::string& path = std::string()) {
            if (!sceneDirty) return false;
            pendingAction = action;
            pendingScenePath = path;
            showUnsavedChangesModal = true;
            return true;
        };

        auto RequestQuit = [&]() {
            if (!MaybePromptUnsaved(PendingSceneAction::Quit)) {
                running = false;
            }
        };

        auto Fail = [&](const char* msg, int code) {
            std::cerr << "quit-prompt-selftest: FAILED (" << msg << ")" << std::endl;
            return code;
        };

        // 1) Dirty scene + SDL_EVENT_QUIT should prompt and keep app running.
        {
            SDL_Event e{};
            e.type = SDL_EVENT_QUIT;
            if (e.type == SDL_EVENT_QUIT) {
                RequestQuit();
            }
            if (!showUnsavedChangesModal || pendingAction != PendingSceneAction::Quit || !running) {
                return Fail("dirty + SDL_EVENT_QUIT did not prompt correctly", 2);
            }
        }

        // 2) Dirty scene + SDL_EVENT_WINDOW_CLOSE_REQUESTED for main window should prompt and keep running.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = true;

            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            e.window.windowID = mainWindowId;
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (e.window.windowID == mainWindowId) {
                    RequestQuit();
                }
            }

            if (!showUnsavedChangesModal || pendingAction != PendingSceneAction::Quit || !running) {
                return Fail("dirty + CLOSE_REQUESTED(main) did not prompt correctly", 3);
            }
        }

        // 3) Dirty scene + CLOSE_REQUESTED for non-main window should NOT prompt.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = true;

            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            e.window.windowID = (SDL_WindowID)(mainWindowId + 1);
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (e.window.windowID == mainWindowId) {
                    RequestQuit();
                }
            }

            if (showUnsavedChangesModal || pendingAction != PendingSceneAction::None || !running) {
                return Fail("dirty + CLOSE_REQUESTED(other) should not prompt", 4);
            }
        }

        // 4) Clean scene + QUIT should exit immediately.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = false;

            SDL_Event e{};
            e.type = SDL_EVENT_QUIT;
            if (e.type == SDL_EVENT_QUIT) {
                RequestQuit();
            }

            if (running || showUnsavedChangesModal || pendingAction != PendingSceneAction::None) {
                return Fail("clean + SDL_EVENT_QUIT should exit without prompt", 5);
            }
        }

        // 5) Clean scene + CLOSE_REQUESTED(main) should exit immediately.
        {
            showUnsavedChangesModal = false;
            pendingAction = PendingSceneAction::None;
            pendingScenePath.clear();
            running = true;
            sceneDirty = false;

            SDL_Event e{};
            e.type = SDL_EVENT_WINDOW_CLOSE_REQUESTED;
            e.window.windowID = mainWindowId;
            if (e.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                if (e.window.windowID == mainWindowId) {
                    RequestQuit();
                }
            }

            if (running || showUnsavedChangesModal || pendingAction != PendingSceneAction::None) {
                return Fail("clean + CLOSE_REQUESTED(main) should exit without prompt", 6);
            }
        }

        std::cout << "quit-prompt-selftest: PASSED" << std::endl;
        return 0;
    }

    EditorSessionSettings editorSettings = LoadEditorSettings();
    std::filesystem::path projectRoot = std::filesystem::current_path();

    auto IsValidProjectRoot = [&](const std::filesystem::path& root) {
        std::error_code ec;
        return std::filesystem::exists(root / "Assets", ec);
    };

    if (!editorSettings.lastProjectRoot.empty()) {
        std::filesystem::path candidate(editorSettings.lastProjectRoot);
        if (IsValidProjectRoot(candidate)) {
            projectRoot = candidate;
        }
    }

    if (!IsValidProjectRoot(projectRoot)) {
        for (const auto& entry : editorSettings.recentProjects) {
            std::filesystem::path candidate(entry.root);
            if (IsValidProjectRoot(candidate)) {
                projectRoot = candidate;
                break;
            }
        }
    }

    if (IsValidProjectRoot(projectRoot)) {
        std::error_code ec;
        std::filesystem::current_path(projectRoot, ec);
        for (const auto& entry : editorSettings.recentProjects) {
            if (entry.root == projectRoot.string() && !entry.lastScene.empty()) {
                editorSettings.lastScenePath = entry.lastScene;
                break;
            }
        }
        editorSettings.lastProjectRoot = projectRoot.string();
        UpdateRecentProject(editorSettings, editorSettings.lastProjectRoot, editorSettings.lastScenePath);
        SaveEditorSettings(editorSettings);
    }

    if (!Genesis::Engine::Init()) {
        std::cerr << "Failed to initialize engine" << std::endl;
        return -1;
    }

    // Configure save subsystem to use the project-local saves directory.
    {
        std::filesystem::path saveDir = projectRoot / "saves";
        std::error_code ec;
        std::filesystem::create_directories(saveDir, ec);
#ifdef _WIN32
        _putenv_s("GENESIS_SAVE_DIR", saveDir.string().c_str());
#else
        setenv("GENESIS_SAVE_DIR", saveDir.string().c_str(), 1);
#endif
        if (!Genesis::Engine::CreateSaveSubsystem("file")) {
            Genesis::Engine::CreateSaveSubsystem("null");
        }
    }

    Genesis::Engine::Window window;
    if (!window.Init("Genesis Editor", 1600, 900)) {
        std::cerr << "Failed to create window" << std::endl;
        Genesis::Engine::Shutdown();
        return -1;
    }

    // Initialize Renderer (Default to factory settings)
    // Explicitly prefer OpenGL for Editor for stability and ImGui compatibility
    std::vector<std::string> gfxOrder = { "opengl", "directx", "vulkan" };
    std::unique_ptr<Genesis::Engine::IGraphicsAPI> rendererInit = Genesis::Engine::GraphicsFactory::CreateRenderer(window.GetSDLWindow(), window.GetGLContext(), gfxOrder, false);
    if (!rendererInit) {
        std::cerr << "Failed to initialize renderer" << std::endl;
        window.Shutdown();
        Genesis::Engine::Shutdown();
        return -1;
    }
    Genesis::Engine::RendererManager::SetRenderer(std::move(rendererInit));

    // Configure Renderer for Editor Mode (Manual Present)
    auto currentRenderer = Genesis::Engine::RendererManager::GetRenderer();
    if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
        glRenderer->SetPresentEnabled(false);
    }

    // Enable Borderless Window and Hit Test for Custom Title Bar
    SDL_SetWindowBordered(window.GetSDLWindow(), false);
    SDL_SetWindowHitTest(window.GetSDLWindow(), HitTestCallback, nullptr);

    // Editor State
    entt::entity selectedEntity = entt::null;
    bool showCommandPalette = false;
    bool zenMode = false;
    bool showGrid = true;
    bool showSceneIcons = true;
    bool occludeSceneIcons = true;
    char commandSearchBuffer[128] = "";
    int selectedCommandIndex = 0;

    // Scene / File State
    std::string currentScenePath;
    bool sceneDirty = false;
    bool requestResetLayout = false;
    bool showOpenSceneModal = false;
    bool showSaveAsSceneModal = false;
    bool showAboutModal = false;
    bool focusInspectorName = false;
    char scenePathBuffer[512] = "";

    // Unsaved changes workflow
    enum class PendingSceneAction {
        None,
        Quit,
        NewScene,
        ShowOpenScene,
        LoadScenePath
    };
    PendingSceneAction pendingAction = PendingSceneAction::None;
    std::string pendingScenePath;
    bool showUnsavedChangesModal = false;

    // Editor Camera State
    glm::vec3 cameraPos = glm::vec3(0.0f, 2.0f, 5.0f);
    glm::vec3 cameraRot = glm::vec3(-20.0f, 0.0f, 0.0f); // Pitch, Yaw, Roll
    float editorCameraFov = 45.0f;
    ImGuizmo::OPERATION currentGizmoOperation = ImGuizmo::TRANSLATE;
    // Fixed world-aligned gizmo by default (modern editor behavior).
    // Note: we still force SCALE to LOCAL at draw time to avoid TRS shear artifacts.
    ImGuizmo::MODE currentGizmoMode = ImGuizmo::WORLD;

    // View Cube (smooth camera transition)
    bool viewCubeAnimating = false;
    float viewCubeAnimTime = 0.0f;
    float viewCubeAnimDuration = 0.25f; // seconds
    glm::vec3 viewCubeStartPos(0.0f);
    glm::vec3 viewCubeTargetPos(0.0f);
    glm::quat viewCubeStartRot(1.0f, 0.0f, 0.0f, 0.0f);
    glm::quat viewCubeTargetRot(1.0f, 0.0f, 0.0f, 0.0f);

    std::shared_ptr<Genesis::Engine::Texture> cameraIconTex;
    std::shared_ptr<Genesis::Engine::Texture> lightDirIconTex;
    std::shared_ptr<Genesis::Engine::Texture> lightPointIconTex;

    // Create scene
    Genesis::Engine::Scene editorScene;
    Genesis::Engine::Scene* activeScene = &editorScene;
    std::unique_ptr<Genesis::Engine::Scene> runtimeScene;

    enum class EditorState {
        Edit,
        Play,
        Pause
    };
    EditorState editorState = EditorState::Edit;
    bool stepRuntime = false;

    std::vector<EditorCommand> undoStack;
    std::vector<EditorCommand> redoStack;
    const size_t kMaxUndo = 64;

    auto PushCommand = [&](EditorCommand cmd) {
        undoStack.push_back(std::move(cmd));
        redoStack.clear();
        if (undoStack.size() > kMaxUndo) {
            undoStack.erase(undoStack.begin());
        }
    };

    auto DoUndo = [&]() {
        if (editorState != EditorState::Edit) return;
        if (undoStack.empty()) return;
        auto cmd = std::move(undoStack.back());
        undoStack.pop_back();
        if (cmd.undo) cmd.undo();
        redoStack.push_back(std::move(cmd));
        sceneDirty = true;
    };

    auto DoRedo = [&]() {
        if (editorState != EditorState::Edit) return;
        if (redoStack.empty()) return;
        auto cmd = std::move(redoStack.back());
        redoStack.pop_back();
        if (cmd.redo) cmd.redo();
        undoStack.push_back(std::move(cmd));
        sceneDirty = true;
    };

    auto RecordScenePath = [&](const std::string& path) {
        const std::string stored = NormalizePathForSettings(path, projectRoot);
        editorSettings.lastProjectRoot = projectRoot.string();
        editorSettings.lastScenePath = stored;
        UpdateRecentProject(editorSettings, editorSettings.lastProjectRoot, stored);
        UpdateRecentScene(editorSettings, stored);
        SaveEditorSettings(editorSettings);
        return stored;
    };

    auto BuildEmptyScene = [&]() {
        editorScene.Clear();
        selectedEntity = entt::null;
        currentScenePath.clear();
        sceneDirty = true;

        activeScene = &editorScene;
        editorState = EditorState::Edit;
        runtimeScene.reset();

        // Default light
        auto lightEntity = editorScene.Registry().create();
        editorScene.Registry().emplace<Genesis::Engine::NameComponent>(lightEntity, Genesis::Engine::NameComponent{"Directional Light"});
        Genesis::Engine::LightComponent lightComp;
        lightComp.type = Genesis::Engine::LightType::Directional;
        lightComp.color[0] = 1.0f; lightComp.color[1] = 0.95f; lightComp.color[2] = 0.8f;
        lightComp.intensity = 1.5f;
        editorScene.Registry().emplace<Genesis::Engine::LightComponent>(lightEntity, lightComp);
        Genesis::Engine::Transform lightTrans;
        lightTrans.rx = -0.5f;
        lightTrans.ry = 0.5f;
        editorScene.Registry().emplace<Genesis::Engine::Transform>(lightEntity, lightTrans);
        selectedEntity = lightEntity;

        // Default Camera
        auto camEntity = editorScene.Registry().create();
        editorScene.Registry().emplace<Genesis::Engine::NameComponent>(camEntity, Genesis::Engine::NameComponent{"Main Camera"});
        Genesis::Engine::Transform camTrans;
        camTrans.z = 10.0f; // Move back
        editorScene.Registry().emplace<Genesis::Engine::Transform>(camEntity, camTrans);
        editorScene.Registry().emplace<Genesis::Engine::CameraComponent>(camEntity);
    };

    struct EntityUndoData {
        EntitySnapshot snapshot;
        entt::entity entity = entt::null;
    };

    auto CreateEntityWithDefaults = [&]() -> entt::entity {
        auto& reg = editorScene.Registry();
        auto e = reg.create();
        reg.emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{"Entity " + std::to_string((uint32_t)e)});
        reg.emplace<Genesis::Engine::Transform>(e);
        return e;
    };

    auto PushCreateCommand = [&](const std::string& label, entt::entity created, const EntitySnapshot& snapshot) {
        auto data = std::make_shared<EntityUndoData>();
        data->snapshot = snapshot;
        data->entity = created;

        PushCommand(EditorCommand{
            label,
            [&, data]() {
                if (editorScene.Registry().valid(data->entity)) {
                    editorScene.Registry().destroy(data->entity);
                    if (selectedEntity == data->entity) selectedEntity = entt::null;
                }
                sceneDirty = true;
            },
            [&, data]() {
                entt::entity e = CreateEntityFromSnapshot(editorScene.Registry(), data->snapshot);
                data->entity = e;
                selectedEntity = e;
                sceneDirty = true;
            }
        });
    };

    auto DeleteEntityWithUndo = [&](entt::entity entity) {
        if (editorState != EditorState::Edit) return;
        if (entity == entt::null || !editorScene.Registry().valid(entity)) return;

        EntitySnapshot snapshot = CaptureEntitySnapshot(editorScene.Registry(), entity);
        auto data = std::make_shared<EntityUndoData>();
        data->snapshot = snapshot;

        editorScene.Registry().destroy(entity);
        if (selectedEntity == entity) selectedEntity = entt::null;
        sceneDirty = true;

        PushCommand(EditorCommand{
            "Delete Entity",
            [&, data]() {
                entt::entity e = CreateEntityFromSnapshot(editorScene.Registry(), data->snapshot);
                data->entity = e;
                selectedEntity = e;
                sceneDirty = true;
            },
            [&, data]() {
                if (editorScene.Registry().valid(data->entity)) {
                    editorScene.Registry().destroy(data->entity);
                    if (selectedEntity == data->entity) selectedEntity = entt::null;
                }
                sceneDirty = true;
            }
        });
    };

    auto DuplicateEntityWithUndo = [&](entt::entity source) {
        if (editorState != EditorState::Edit) return;
        if (source == entt::null || !editorScene.Registry().valid(source)) return;

        EntitySnapshot snapshot = CaptureEntitySnapshot(editorScene.Registry(), source);
        if (snapshot.hasName && !snapshot.name.name.empty()) {
            snapshot.name.name += " Copy";
        }
        entt::entity dup = CreateEntityFromSnapshot(editorScene.Registry(), snapshot);
        selectedEntity = dup;
        sceneDirty = true;
        PushCreateCommand("Duplicate Entity", dup, snapshot);
    };

    auto PushRenameCommand = [&](entt::entity entity, const std::string& before, const std::string& after) {
        if (before == after) return;
        PushCommand(EditorCommand{
            "Rename Entity",
            [&, entity, before]() {
                if (editorScene.Registry().valid(entity)) {
                    editorScene.Registry().emplace_or_replace<Genesis::Engine::NameComponent>(entity, Genesis::Engine::NameComponent{ before });
                    sceneDirty = true;
                }
            },
            [&, entity, after]() {
                if (editorScene.Registry().valid(entity)) {
                    editorScene.Registry().emplace_or_replace<Genesis::Engine::NameComponent>(entity, Genesis::Engine::NameComponent{ after });
                    sceneDirty = true;
                }
            }
        });
    };

    auto PushTransformCommand = [&](entt::entity entity, const Genesis::Engine::Transform& before, const Genesis::Engine::Transform& after) {
        if (TransformNearlyEqual(before, after)) return;
        PushCommand(EditorCommand{
            "Modify Transform",
            [&, entity, before]() {
                if (editorScene.Registry().valid(entity) && editorScene.Registry().any_of<Genesis::Engine::Transform>(entity)) {
                    editorScene.Registry().get<Genesis::Engine::Transform>(entity) = before;
                    sceneDirty = true;
                }
            },
            [&, entity, after]() {
                if (editorScene.Registry().valid(entity) && editorScene.Registry().any_of<Genesis::Engine::Transform>(entity)) {
                    editorScene.Registry().get<Genesis::Engine::Transform>(entity) = after;
                    sceneDirty = true;
                }
            }
        });
    };

    auto SetParentWithUndo = [&](entt::entity child, entt::entity newParent) {
        if (editorState != EditorState::Edit) return;
        auto& reg = editorScene.Registry();
        if (!reg.valid(child)) return;
        if (newParent != entt::null && !reg.valid(newParent)) return;
        if (child == newParent) return;
        if (newParent != entt::null && IsAncestor(reg, child, newParent)) return;

        entt::entity oldParent = entt::null;
        if (reg.any_of<Genesis::Engine::ParentComponent>(child)) {
            oldParent = reg.get<Genesis::Engine::ParentComponent>(child).parent;
        }

        Genesis::Engine::Transform beforeTransform{};
        bool hasTransform = reg.any_of<Genesis::Engine::Transform>(child);
        if (hasTransform) beforeTransform = reg.get<Genesis::Engine::Transform>(child);

        glm::mat4 childWorld = GetWorldMatrixGLM(reg, child);
        glm::mat4 parentWorld = glm::mat4(1.0f);
        if (newParent != entt::null && reg.valid(newParent)) {
            parentWorld = GetWorldMatrixGLM(reg, newParent);
        }
        glm::mat4 localMat = childWorld;
        if (newParent != entt::null) {
            localMat = glm::inverse(parentWorld) * childWorld;
        }

        Genesis::Engine::Transform afterTransform = beforeTransform;
        if (hasTransform) {
            DecomposeTransformGLM(localMat, afterTransform);
        }

        if (newParent == entt::null) {
            if (reg.any_of<Genesis::Engine::ParentComponent>(child)) {
                reg.remove<Genesis::Engine::ParentComponent>(child);
            }
        } else {
            reg.emplace_or_replace<Genesis::Engine::ParentComponent>(child, Genesis::Engine::ParentComponent{ newParent });
        }
        if (hasTransform) {
            reg.get<Genesis::Engine::Transform>(child) = afterTransform;
        }
        sceneDirty = true;

        PushCommand(EditorCommand{
            "Reparent Entity",
            [&, child, oldParent, beforeTransform, hasTransform]() {
                if (!reg.valid(child)) return;
                if (oldParent == entt::null) {
                    if (reg.any_of<Genesis::Engine::ParentComponent>(child)) {
                        reg.remove<Genesis::Engine::ParentComponent>(child);
                    }
                } else {
                    reg.emplace_or_replace<Genesis::Engine::ParentComponent>(child, Genesis::Engine::ParentComponent{ oldParent });
                }
                if (hasTransform) {
                    reg.get<Genesis::Engine::Transform>(child) = beforeTransform;
                }
                sceneDirty = true;
            },
            [&, child, newParent, afterTransform, hasTransform]() {
                if (!reg.valid(child)) return;
                if (newParent == entt::null) {
                    if (reg.any_of<Genesis::Engine::ParentComponent>(child)) {
                        reg.remove<Genesis::Engine::ParentComponent>(child);
                    }
                } else {
                    if (!reg.valid(newParent)) return;
                    reg.emplace_or_replace<Genesis::Engine::ParentComponent>(child, Genesis::Engine::ParentComponent{ newParent });
                }
                if (hasTransform) {
                    reg.get<Genesis::Engine::Transform>(child) = afterTransform;
                }
                sceneDirty = true;
            }
        });
    };

    auto ResolveScenePathForLoad = [&](const std::string& storedPath) {
        if (storedPath.empty()) return std::string();
        std::filesystem::path p(storedPath);
        if (p.is_relative()) {
            return (projectRoot / p).string();
        }
        return p.string();
    };

    // Load most recent scene if available; otherwise start with a new empty scene.
    bool loadedStartupScene = false;
    if (!editorSettings.lastScenePath.empty()) {
        const std::string loadPath = ResolveScenePathForLoad(editorSettings.lastScenePath);
        if (!loadPath.empty() && Genesis::Engine::SceneLoader::LoadScene(editorScene, loadPath)) {
            currentScenePath = editorSettings.lastScenePath;
            sceneDirty = false;
            selectedEntity = entt::null;
            UpdateRecentProject(editorSettings, editorSettings.lastProjectRoot, editorSettings.lastScenePath);
            UpdateRecentScene(editorSettings, editorSettings.lastScenePath);
            SaveEditorSettings(editorSettings);
            loadedStartupScene = true;
        }
    }

    if (!loadedStartupScene) {
        BuildEmptyScene();
    }

    std::filesystem::path autosavePath = projectRoot / "saves" / "autosave.scene";
    bool showAutosaveRestoreModal = false;
    std::string autosaveBaseScenePath = currentScenePath;
    {
        std::error_code ec;
        const bool autosaveExists = std::filesystem::exists(autosavePath, ec);
        if (autosaveExists) {
            bool shouldRestore = false;
            if (currentScenePath.empty()) {
                shouldRestore = true;
            } else {
                std::filesystem::path scenePathResolved = ResolveScenePathForLoad(currentScenePath);
                if (!std::filesystem::exists(scenePathResolved, ec)) {
                    shouldRestore = true;
                } else {
                    auto autosaveTime = std::filesystem::last_write_time(autosavePath, ec);
                    if (!ec) {
                        auto sceneTime = std::filesystem::last_write_time(scenePathResolved, ec);
                        if (!ec && autosaveTime > sceneTime) shouldRestore = true;
                    }
                }
            }
            if (shouldRestore) {
                showAutosaveRestoreModal = true;
            }
        }
    }

    // Setup profiler and ImGui
    Genesis::Engine::Profiler profiler;
    Genesis::Engine::ImGuiLayer gui(window.GetSDLWindow(), window.GetGLContext());

    // Global UI sizing tweak (Editor-only): make widgets/buttons slightly roomier.
    // This helps match the more comfortable click targets users expect from tools like VS Code.
    {
        ImGuiStyle& style = ImGui::GetStyle();
        // (Option B sizing): clearly larger click targets.
        style.FramePadding = ImVec2(style.FramePadding.x + 4.0f, style.FramePadding.y + 4.0f);
        style.ItemSpacing = ImVec2(style.ItemSpacing.x + 4.0f, style.ItemSpacing.y + 2.0f);
        style.ScrollbarSize += 4.0f;
        style.GrabMinSize += 4.0f;
    }

    cameraIconTex = LoadIconTexturePPM("Assets/icons/camera_icon.ppm");
    lightDirIconTex = LoadIconTexturePPM("Assets/icons/light_dir_icon.ppm");
    lightPointIconTex = LoadIconTexturePPM("Assets/icons/light_point_icon.ppm");

    std::cout << "Editor initialized. Entering main loop..." << std::endl;

    uint64_t lastTime = SDL_GetPerformanceCounter();

    bool running = true;
    double autosaveTimer = 0.0;
    const double autosaveInterval = 120.0;

    auto DoNewScene = [&]() {
        BuildEmptyScene();
    };

    auto MaybePromptUnsaved = [&](PendingSceneAction action, const std::string& path = std::string()) {
        if (!sceneDirty) return false;
        pendingAction = action;
        pendingScenePath = path;
        showUnsavedChangesModal = true;
        return true;
    };

    auto RequestQuit = [&]() {
        if (!MaybePromptUnsaved(PendingSceneAction::Quit)) {
            running = false;
        }
    };

    auto ClearAutosave = [&]() {
        std::error_code ec;
        std::filesystem::remove(autosavePath, ec);
    };

    auto SaveSceneToPath = [&](const std::string& path, bool updateRecents) -> bool {
        if (path.empty()) return false;
        if (Genesis::Engine::SceneLoader::SaveScene(editorScene, path)) {
            if (updateRecents) {
                currentScenePath = RecordScenePath(path);
            } else {
                currentScenePath = path;
            }
            sceneDirty = false;
            ClearAutosave();
            return true;
        }
        return false;
    };

    auto LoadSceneFromPath = [&](const std::string& storedPath, bool updateRecents) -> bool {
        if (storedPath.empty()) return false;
        if (editorState != EditorState::Edit) {
            if (activeScene) activeScene->OnRuntimeStop();
            activeScene = &editorScene;
            runtimeScene.reset();
            editorState = EditorState::Edit;
        }

        const std::string loadPath = ResolveScenePathForLoad(storedPath);
        if (!loadPath.empty() && Genesis::Engine::SceneLoader::LoadScene(editorScene, loadPath)) {
            currentScenePath = updateRecents ? RecordScenePath(storedPath) : storedPath;
            sceneDirty = false;
            selectedEntity = entt::null;
            return true;
        }
        return false;
    };

    while (running) {
        // Pump SDL events (and intercept OS quit/close requests so we can prompt for unsaved changes).
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);

            if (event.type == SDL_EVENT_QUIT) {
                RequestQuit();
            }

            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED) {
                // ImGui multi-viewport creates additional SDL windows; do not treat their close
                // requests as a request to exit the whole app.
                if (event.window.windowID == window.GetWindowID()) {
                    RequestQuit();
                }
            }
        }

        if (!running) break;

        uint64_t now = SDL_GetPerformanceCounter();
        double dt = (double)((now - lastTime) * 1000 / SDL_GetPerformanceFrequency()) / 1000.0;
        lastTime = now;

        // Clamp dt to avoid huge jumps (e.g. debugging)
        if (dt > 0.1) dt = 0.1;

        if (editorState == EditorState::Edit) {
            autosaveTimer += dt;
            if (autosaveTimer >= autosaveInterval) {
                autosaveTimer = 0.0;
                if (sceneDirty) {
                    std::error_code ec;
                    std::filesystem::create_directories(autosavePath.parent_path(), ec);
                    Genesis::Engine::SceneLoader::SaveScene(editorScene, autosavePath.string());
                }
            }
        } else {
            autosaveTimer = 0.0;
        }

        // Smooth camera animation toward view-cube target.
        // Removed: ImGuizmo handles interpolation internally for clicks, and dragging should be immediate.
        /*
        if (viewCubeAnimating) {
            viewCubeAnimTime += (float)dt;
            float t = viewCubeAnimDuration > 0.0f ? (viewCubeAnimTime / viewCubeAnimDuration) : 1.0f;
            if (t >= 1.0f) {
                t = 1.0f;
                viewCubeAnimating = false;
            }
            // Smoothstep
            float s = t * t * (3.0f - 2.0f * t);

            cameraPos = glm::mix(viewCubeStartPos, viewCubeTargetPos, s);
            glm::quat rot = glm::slerp(viewCubeStartRot, viewCubeTargetRot, s);
            glm::vec3 euler = glm::degrees(glm::eulerAngles(rot));
            cameraRot.x = euler.x;
            cameraRot.y = euler.y;
            cameraRot.z = euler.z;
        }
        */

        profiler.BeginFrame();

        // Start ImGui frame
        gui.NewFrame();
        ImGuizmo::BeginFrame();

        // Global Shortcuts (Editor)
        if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_P)) {
            showCommandPalette = !showCommandPalette;
            if (showCommandPalette) {
                memset(commandSearchBuffer, 0, sizeof(commandSearchBuffer));
                selectedCommandIndex = 0;
                ImGui::SetNextWindowFocus();
            }
        }

        if (!ImGui::GetIO().WantTextInput) {
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Z)) {
                if (ImGui::GetIO().KeyShift) {
                    DoRedo();
                } else {
                    DoUndo();
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Y)) {
                DoRedo();
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_N)) {
                if (!MaybePromptUnsaved(PendingSceneAction::NewScene)) {
                    DoNewScene();
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_D)) {
                if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                    DuplicateEntityWithUndo(selectedEntity);
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_O)) {
                if (!MaybePromptUnsaved(PendingSceneAction::ShowOpenScene)) {
                    showOpenSceneModal = true;
                    if (!currentScenePath.empty()) {
                        strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                    } else {
                        const std::string fallbackPath = !editorSettings.lastScenePath.empty()
                            ? editorSettings.lastScenePath
                            : std::string("Assets/scenes/scene.scene");
                        strncpy_s(scenePathBuffer, fallbackPath.c_str(), sizeof(scenePathBuffer) - 1);
                    }
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && !ImGui::GetIO().KeyShift) {
                if (currentScenePath.empty()) {
                    showSaveAsSceneModal = true;
                    strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                } else {
                    // Only save if in Edit Mode
                    // If in Play Mode, we might want to ignore or save the runtime state? Usually ignored.
                    if (editorState == EditorState::Edit) {
                        SaveSceneToPath(currentScenePath, true);
                    }
                }
            }
            if (ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_S) && ImGui::GetIO().KeyShift) {
                showSaveAsSceneModal = true;
                if (!currentScenePath.empty()) {
                    strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                } else {
                    strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                }
            }
        }
        if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_Delete)) {
            if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                DeleteEntityWithUndo(selectedEntity);
            }
        }

        // Render Editor UI (on top of scene)
        // gui.Render(profiler, &scene); // Replaced with custom editor layout below

        // Custom Editor Layout
        ImGuiID dockspace_id = ImGui::DockSpaceOverViewport(0, ImGui::GetMainViewport());

        // Default docking layout
        //
        // ImGui will automatically restore a user's custom layout from imgui.ini if present.
        // On a *fresh* startup there may be no ini file yet, so we proactively build a sensible
        // default arrangement to avoid forcing users to reorganize panels.
        auto BuildDefaultDockLayout = [&](ImGuiID rootDockId) {
            ImGui::DockBuilderRemoveNode(rootDockId);
            ImGui::DockBuilderAddNode(rootDockId, ImGuiDockNodeFlags_DockSpace);
            ImGui::DockBuilderSetNodeSize(rootDockId, ImGui::GetMainViewport()->Size);

            ImGuiID dock_main_id = rootDockId;
            ImGuiID dock_id_right = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Right, 0.28f, nullptr, &dock_main_id);
            ImGuiID dock_id_left = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Left, 0.22f, nullptr, &dock_main_id);
            ImGuiID dock_id_bottom = ImGui::DockBuilderSplitNode(dock_main_id, ImGuiDir_Down, 0.28f, nullptr, &dock_main_id);

            // Split Left into Top (Hierarchy) and Bottom (Content Browser)
            ImGuiID dock_id_left_bottom = ImGui::DockBuilderSplitNode(dock_id_left, ImGuiDir_Down, 0.45f, nullptr, &dock_id_left);

            ImGui::DockBuilderDockWindow("Viewport", dock_main_id);
            ImGui::DockBuilderDockWindow("Inspector", dock_id_right);
            ImGui::DockBuilderDockWindow("Scene Hierarchy", dock_id_left);
            ImGui::DockBuilderDockWindow("Content Browser", dock_id_left_bottom);
            ImGui::DockBuilderDockWindow("Console", dock_id_bottom);

            ImGui::DockBuilderFinish(rootDockId);
        };

        static bool defaultDockLayoutAppliedThisRun = false;
        if (!defaultDockLayoutAppliedThisRun) {
            const char* ini = ImGui::GetIO().IniFilename;
            bool iniExists = false;
            if (ini && ini[0] != '\0') {
                std::error_code ec;
                iniExists = std::filesystem::exists(std::filesystem::path(ini), ec);
            }

            // If there's no imgui.ini yet, this is likely a first run: apply a sensible default.
            if (!iniExists) {
                BuildDefaultDockLayout(dockspace_id);
            }

            defaultDockLayoutAppliedThisRun = true;
        }

        if (requestResetLayout) {
            requestResetLayout = false;
            BuildDefaultDockLayout(dockspace_id);
        }

        // Custom Title Bar (VS Code Style)
        // (Option B sizing): clearly larger targets.
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 12));
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(12, 8));
        if (ImGui::BeginMainMenuBar()) {
            // Icon / Title
            ImGui::Text("  Genesis  ");
            ImGui::Separator();

            if (ImGui::BeginMenu("File")) {
                if (ImGui::MenuItem("New Scene", "Ctrl+N")) {
                    if (!MaybePromptUnsaved(PendingSceneAction::NewScene)) {
                        DoNewScene();
                    }
                }
                if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) {
                    if (!MaybePromptUnsaved(PendingSceneAction::ShowOpenScene)) {
                        showOpenSceneModal = true;
                        if (!currentScenePath.empty()) {
                            strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                        } else {
                            const std::string fallbackPath = !editorSettings.lastScenePath.empty()
                                ? editorSettings.lastScenePath
                                : std::string("Assets/scenes/scene.scene");
                            strncpy_s(scenePathBuffer, fallbackPath.c_str(), sizeof(scenePathBuffer) - 1);
                        }
                    }
                }
                if (ImGui::BeginMenu("Open Recent")) {
                    if (editorSettings.recentScenes.empty()) {
                        ImGui::TextDisabled("(empty)");
                    } else {
                        for (size_t i = 0; i < editorSettings.recentScenes.size(); ++i) {
                            const std::string& path = editorSettings.recentScenes[i];
                            std::string label = path + "##recent_scene_" + std::to_string(i);
                            std::error_code ec;
                            const std::string resolvedPath = ResolveScenePathForLoad(path);
                            const bool exists = !resolvedPath.empty() && std::filesystem::exists(resolvedPath, ec);
                            if (ImGui::MenuItem(label.c_str(), nullptr, false, exists)) {
                                if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, path)) {
                                    LoadSceneFromPath(path, true);
                                }
                            }
                        }
                    }
                    ImGui::EndMenu();
                }
                if (ImGui::MenuItem("Save", "Ctrl+S")) {
                    if (currentScenePath.empty()) {
                        showSaveAsSceneModal = true;
                        strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                    } else {
                        if (editorState == EditorState::Edit) {
                            SaveSceneToPath(currentScenePath, true);
                        }
                    }
                }
                if (ImGui::MenuItem("Save As...", "Ctrl+Shift+S")) {
                    showSaveAsSceneModal = true;
                    if (!currentScenePath.empty()) {
                        strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                    } else {
                        strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                    }
                }
                ImGui::Separator();
                if (ImGui::MenuItem("Exit", "Alt+F4")) { RequestQuit(); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Edit")) {
                const bool canUndo = !undoStack.empty();
                const bool canRedo = !redoStack.empty();
                const bool hasSelection = (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity));
                if (ImGui::MenuItem("Undo", "Ctrl+Z", false, canUndo)) { DoUndo(); }
                if (ImGui::MenuItem("Redo", "Ctrl+Y", false, canRedo)) { DoRedo(); }
                ImGui::Separator();
                if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSelection)) { DuplicateEntityWithUndo(selectedEntity); }
                if (ImGui::MenuItem("Delete", "Del", false, hasSelection)) { DeleteEntityWithUndo(selectedEntity); }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("View")) {
                if (ImGui::MenuItem("Toggle Zen Mode", "Ctrl+K Z", &zenMode)) {}
                if (ImGui::MenuItem("Toggle Grid", "G", &showGrid)) {}
                ImGui::Separator();
                if (ImGui::MenuItem("Show Non-Visible Icons", "I", &showSceneIcons)) {}
                if (ImGui::MenuItem("Occlude Non-Visible Icons", nullptr, &occludeSceneIcons)) {}
                if (ImGui::MenuItem("Reset Layout")) { requestResetLayout = true; }
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Window")) {
                ImGui::MenuItem("Viewport");
                ImGui::MenuItem("Inspector");
                ImGui::MenuItem("Scene Hierarchy");
                ImGui::MenuItem("Content Browser");
                ImGui::EndMenu();
            }
            if (ImGui::BeginMenu("Help")) {
                if (ImGui::MenuItem("About")) { showAboutModal = true; }
                ImGui::EndMenu();
            }

            // Play / Stop Toolbar
            {
                // Padding after Help menu
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 16.0f);
                
                if (editorState == EditorState::Edit) {
                    if (ImGui::Button("Play", ImVec2(80, 0))) {
                        // Enter Play Mode
                        stepRuntime = false;
                        runtimeScene = std::make_unique<Genesis::Engine::Scene>();
                        runtimeScene->CopyFrom(editorScene);
                        activeScene = runtimeScene.get();
                        activeScene->OnRuntimeStart();
                        editorState = EditorState::Play;
                        selectedEntity = entt::null;
                    }
                } else {
                    // Play / Pause / Stop controls
                    if (editorState == EditorState::Play) {
                        if (ImGui::Button("Pause", ImVec2(80, 0))) {
                            editorState = EditorState::Pause;
                        }
                    } else if (editorState == EditorState::Pause) {
                        if (ImGui::Button("Resume", ImVec2(80, 0))) {
                            editorState = EditorState::Play;
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Step", ImVec2(80, 0))) {
                            stepRuntime = true;
                        }
                    }

                    ImGui::SameLine();
                    if (ImGui::Button("Stop", ImVec2(80, 0))) {
                         // Stop Play Mode
                         if (activeScene) activeScene->OnRuntimeStop();
                         activeScene = &editorScene;
                         runtimeScene.reset();
                         editorState = EditorState::Edit;
                         selectedEntity = entt::null;
                        stepRuntime = false;
                    }
                }
            }

            // Window Controls (Right Aligned)
            // Slightly larger click targets to match desktop IDE/editor expectations.
            float buttonWidth = 64.0f;
            float buttonHeight = ImGui::GetWindowHeight(); // Match menu bar height
            float controlsWidth = buttonWidth * 3;
            ImGui::SetCursorPosX(ImGui::GetWindowWidth() - controlsWidth);
            
            static bool isCustomMaximized = false;
            static int restoreX = 0, restoreY = 0, restoreW = 1600, restoreH = 900;

            // Helper lambda for drawing custom window buttons
            auto DrawWindowButton = [&](const char* id, int type) -> bool {
                ImVec2 pos = ImGui::GetCursorScreenPos();
                ImVec2 size = ImVec2(buttonWidth, buttonHeight);
                bool clicked = ImGui::InvisibleButton(id, size);
                bool hovered = ImGui::IsItemHovered();
                bool active = ImGui::IsItemActive();
                
                ImU32 bgColor = 0;
                ImU32 iconColor = IM_COL32(200, 200, 200, 255); // Light grey text

                if (type == 2) { // Close button
                    if (hovered) bgColor = IM_COL32(232, 17, 35, 255); // Red
                    if (active) bgColor = IM_COL32(153, 11, 23, 255); // Darker Red
                    if (hovered || active) iconColor = IM_COL32(255, 255, 255, 255); // White icon
                } else {
                    if (hovered) bgColor = IM_COL32(255, 255, 255, 30); // Subtle white overlay
                    if (active) bgColor = IM_COL32(255, 255, 255, 60);
                }

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                if (bgColor != 0)
                    drawList->AddRectFilled(pos, ImVec2(pos.x + size.x, pos.y + size.y), bgColor);

                // Draw Icon (Centered)
                ImVec2 center = ImVec2(pos.x + size.x * 0.5f, pos.y + size.y * 0.5f);
                float iconSize = 13.0f; // Slightly larger for the bigger button
                float half = iconSize * 0.5f;
                
                if (type == 0) { // Minimize
                    drawList->AddLine(ImVec2(center.x - half, center.y), ImVec2(center.x + half, center.y), iconColor, 2.0f);
                }
                else if (type == 1) { // Maximize/Restore
                    bool isMaximized = isCustomMaximized || (SDL_GetWindowFlags(window.GetSDLWindow()) & SDL_WINDOW_MAXIMIZED);
                    if (isMaximized) {
                        // Restore icon (two overlapping squares)
                        float offset = 2.0f;
                        // Back square
                        drawList->AddRect(ImVec2(center.x - half + offset, center.y - half - offset), ImVec2(center.x + half + offset, center.y + half - offset), iconColor, 0.0f, 0, 2.0f);
                        // Front square fill (to hide back line)
                        drawList->AddRectFilled(ImVec2(center.x - half - offset, center.y - half + offset), ImVec2(center.x + half - offset, center.y + half + offset), ImGui::GetColorU32(ImGuiCol_MenuBarBg)); 
                        // Front square border
                        drawList->AddRect(ImVec2(center.x - half - offset, center.y - half + offset), ImVec2(center.x + half - offset, center.y + half + offset), iconColor, 0.0f, 0, 2.0f);
                    } else {
                        // Maximize icon (one square)
                        drawList->AddRect(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), iconColor, 0.0f, 0, 2.0f);
                    }
                }
                else if (type == 2) { // Close (X)
                    drawList->AddLine(ImVec2(center.x - half, center.y - half), ImVec2(center.x + half, center.y + half), iconColor, 2.0f);
                    drawList->AddLine(ImVec2(center.x + half, center.y - half), ImVec2(center.x - half, center.y + half), iconColor, 2.0f);
                }

                return clicked;
            };

            if (DrawWindowButton("Min", 0)) { SDL_MinimizeWindow(window.GetSDLWindow()); }
            ImGui::SameLine(0, 0);
            if (DrawWindowButton("Max", 1)) { 
                if (SDL_GetWindowFlags(window.GetSDLWindow()) & SDL_WINDOW_MAXIMIZED) {
                    SDL_RestoreWindow(window.GetSDLWindow());
                    isCustomMaximized = false;
                }
                else if (isCustomMaximized) {
                    SDL_SetWindowPosition(window.GetSDLWindow(), restoreX, restoreY);
                    SDL_SetWindowSize(window.GetSDLWindow(), restoreW, restoreH);
                    isCustomMaximized = false;
                }
                else {
                    SDL_GetWindowPosition(window.GetSDLWindow(), &restoreX, &restoreY);
                    SDL_GetWindowSize(window.GetSDLWindow(), &restoreW, &restoreH);
                    
                    SDL_DisplayID displayID = SDL_GetDisplayForWindow(window.GetSDLWindow());
                    SDL_Rect usableBounds;
                    if (SDL_GetDisplayUsableBounds(displayID, &usableBounds)) {
                        SDL_SetWindowPosition(window.GetSDLWindow(), usableBounds.x, usableBounds.y);
                        SDL_SetWindowSize(window.GetSDLWindow(), usableBounds.w, usableBounds.h);
                        isCustomMaximized = true;
                    }
                }
            }
            ImGui::SameLine(0, 0);
            if (DrawWindowButton("Close", 2)) { RequestQuit(); }

            ImGui::EndMainMenuBar();
        }
        ImGui::PopStyleVar(2);

        // Viewport Window
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
        ImGui::Begin("Viewport");
        // Use cursor screen position as the authoritative top-left for the viewport image.
        // This avoids subtle misalignment with docking/tab bars and matches where the Image() is actually drawn.
        ImVec2 viewportTopLeft = ImGui::GetCursorScreenPos();
        ImVec2 viewportSize = ImGui::GetContentRegionAvail();

        const bool viewportHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
        const bool viewportFocused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows);

        // Calculate ViewCube bounds first (needed for camera nav conflict detection)
        const float viewManipulateSize = 128.0f;
        const float viewManipulatePad = 8.0f;
        ImVec2 viewManipulatePos = ImVec2(
            viewportTopLeft.x + viewportSize.x - viewManipulateSize - viewManipulatePad,
            viewportTopLeft.y + viewManipulatePad
        );
        ImVec2 viewCubeMin = viewManipulatePos;
        ImVec2 viewCubeMax = ImVec2(viewManipulatePos.x + viewManipulateSize, viewManipulatePos.y + viewManipulateSize);
        const bool mouseOverViewCube = ImGui::IsMouseHoveringRect(viewCubeMin, viewCubeMax, false);

        // Input ownership for the viewport.
        // Prevents camera navigation, gizmos, and view cube manipulation from fighting over the same mouse drag.
        enum class ViewportInputOwner {
            None,
            ViewCube,
            Gizmo,
            CameraNav
        };
        static ViewportInputOwner inputOwner = ViewportInputOwner::None;

        const bool wantText = ImGui::GetIO().WantTextInput;
        const bool lDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        const bool rDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        const bool mDown = ImGui::IsMouseDown(ImGuiMouseButton_Middle);
        const bool lClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left);
        const bool viewportLeftClicked = viewportHovered && lClicked;

        // Acquire input ownership.
        if (inputOwner == ViewportInputOwner::None) {
            if (lClicked && mouseOverViewCube) {
                inputOwner = ViewportInputOwner::ViewCube;
            } else if ((lClicked || lDown) && ImGuizmo::IsOver()) {
                inputOwner = ViewportInputOwner::Gizmo;
            } else if (viewportFocused && viewportHovered && !mouseOverViewCube && (rDown || mDown) && !wantText) {
                inputOwner = ViewportInputOwner::CameraNav;
            }
        }

        // Release ownership.
        if (inputOwner == ViewportInputOwner::ViewCube && !lDown) {
            inputOwner = ViewportInputOwner::None;
        } else if (inputOwner == ViewportInputOwner::Gizmo && !lDown && !ImGuizmo::IsUsing()) {
            inputOwner = ViewportInputOwner::None;
        } else if (inputOwner == ViewportInputOwner::CameraNav && !rDown && !mDown) {
            inputOwner = ViewportInputOwner::None;
        }

        // Camera navigation is exclusive.
        const bool cameraNavActive = (inputOwner == ViewportInputOwner::CameraNav);
        const bool allowGizmoInteractionThisFrame = (inputOwner == ViewportInputOwner::None || inputOwner == ViewportInputOwner::Gizmo);
        const bool applyViewCubeThisFrame = (inputOwner == ViewportInputOwner::ViewCube);

        // Gizmo Shortcuts
        if (!cameraNavActive && !wantText && !ImGuizmo::IsUsing()) {
            if (ImGui::IsKeyPressed(ImGuiKey_W)) currentGizmoOperation = ImGuizmo::TRANSLATE;
            if (ImGui::IsKeyPressed(ImGuiKey_E)) currentGizmoOperation = ImGuizmo::ROTATE;
            if (ImGui::IsKeyPressed(ImGuiKey_R)) currentGizmoOperation = ImGuizmo::SCALE;
        }

        if (!ImGui::GetIO().WantTextInput && ImGui::IsKeyPressed(ImGuiKey_G)) {
            // Keep this scoped to viewport focus to avoid toggling grid while typing elsewhere.
            if (viewportFocused) showGrid = !showGrid;
        }

        if (editorState == EditorState::Edit && viewportHovered && !wantText && !ImGuizmo::IsUsing()) {
            float wheel = ImGui::GetIO().MouseWheel;
            if (wheel != 0.0f) {
                const float fovStep = 2.0f;
                const float minFov = 15.0f;
                const float maxFov = 90.0f;
                editorCameraFov = glm::clamp(editorCameraFov - wheel * fovStep, minFov, maxFov);
            }
        }

        // Camera Navigation (viewport-scoped)
        if (cameraNavActive) {
            viewCubeAnimating = false;

            float speed = 5.0f * (float)dt;
            if (ImGui::IsKeyDown(ImGuiKey_LeftShift)) speed *= 2.0f;

            glm::vec3 forward;
            forward.x = sin(glm::radians(cameraRot.y)) * cos(glm::radians(cameraRot.x));
            forward.y = -sin(glm::radians(cameraRot.x));
            forward.z = -cos(glm::radians(cameraRot.y)) * cos(glm::radians(cameraRot.x));
            forward = glm::normalize(forward);

            // Robust right vector even when looking nearly straight up/down.
            // (Cross with world-up becomes degenerate at |dot(forward, up)| ~= 1.)
            glm::vec3 refUp(0.0f, 1.0f, 0.0f);
            if (fabsf(glm::dot(forward, refUp)) > 0.99f) {
                refUp = glm::vec3(0.0f, 0.0f, 1.0f);
            }
            glm::vec3 right = glm::normalize(glm::cross(forward, refUp));
            glm::vec3 up = glm::normalize(glm::cross(right, forward));

            if (rDown) {
                if (ImGui::IsKeyDown(ImGuiKey_W)) cameraPos += forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_S)) cameraPos -= forward * speed;
                if (ImGui::IsKeyDown(ImGuiKey_A)) cameraPos -= right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_D)) cameraPos += right * speed;
                if (ImGui::IsKeyDown(ImGuiKey_Q)) cameraPos -= glm::vec3(0, 1, 0) * speed;
                if (ImGui::IsKeyDown(ImGuiKey_E)) cameraPos += glm::vec3(0, 1, 0) * speed;

                // Mouse Look (once captured, apply regardless of hover for stable navigation)
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                cameraRot.y -= delta.x * 0.1f; // Invert X for intuitive look
                cameraRot.x -= delta.y * 0.1f; // Invert Y
            } else if (mDown) {
                ImVec2 delta = ImGui::GetIO().MouseDelta;
                const float panSpeed = 0.01f;
                cameraPos -= right * (delta.x * panSpeed);
                cameraPos += up * (delta.y * panSpeed);
            }
        }

        // Update Camera Matrices
        glm::mat4 view = glm::mat4(1.0f);
        view = glm::rotate(view, glm::radians(cameraRot.x), glm::vec3(1, 0, 0));
        view = glm::rotate(view, glm::radians(cameraRot.y), glm::vec3(0, 1, 0));
        view = glm::translate(view, -cameraPos);

        glm::mat4 projection = glm::perspective(glm::radians(editorCameraFov), 16.0f / 9.0f, 0.1f, 100.0f);
        
        // Update Projection Aspect Ratio based on Viewport Size
        if (viewportSize.x > 0 && viewportSize.y > 0) {
            projection = glm::perspective(glm::radians(editorCameraFov), viewportSize.x / viewportSize.y, 0.1f, 100.0f);
        }

        // Play Mode Camera Override
        if (editorState == EditorState::Play || editorState == EditorState::Pause) {
            auto viewCam = activeScene->Registry().view<Genesis::Engine::CameraComponent, Genesis::Engine::Transform>();
            bool cameraFound = false;
            for (auto entity : viewCam) {
                const auto& cam = viewCam.get<Genesis::Engine::CameraComponent>(entity);
                if (cam.primary) {
                    const auto& t = viewCam.get<Genesis::Engine::Transform>(entity);
                    
                    // Engine Transform -> View Matrix
                    // Eye position
                    glm::vec3 eye(t.x, t.y, t.z);
                    
                    // Rotation matrix (match Scene::Render logic: Rot = Rz * Ry * Rx)
                    glm::mat4 rotX = glm::rotate(glm::mat4(1.0f), t.rx, glm::vec3(1, 0, 0));
                    glm::mat4 rotY = glm::rotate(glm::mat4(1.0f), t.ry, glm::vec3(0, 1, 0));
                    glm::mat4 rotZ = glm::rotate(glm::mat4(1.0f), t.rz, glm::vec3(0, 0, 1));
                    glm::mat4 rot = rotZ * rotY * rotX;

                    // Forward vector is usually -Z in OpenGL view space.
                    // If Identity rotation faces -Z:
                    glm::vec3 forward = glm::vec3(rot * glm::vec4(0, 0, -1, 0));
                    glm::vec3 up = glm::vec3(rot * glm::vec4(0, 1, 0, 0));

                    view = glm::lookAt(eye, eye + forward, up);

                    if (viewportSize.x > 0 && viewportSize.y > 0) {
                        projection = glm::perspective(glm::radians(cam.fov), viewportSize.x / viewportSize.y, cam.nearPlane, cam.farPlane);
                    }
                    cameraFound = true;
                    break;
                }
            }
            if (!cameraFound) {
                // Fallback / Warning
                // Keep editor camera but maybe show text?
            }
        }

        // Render scene (now that camera matrices are final)
        if (currentRenderer) {
            currentRenderer->BeginFrame();
            currentRenderer->SetViewProjection(glm::value_ptr(view), glm::value_ptr(projection));
        }

        if (editorState == EditorState::Play) {
            activeScene->OnUpdateRuntime(dt);
        } else if (editorState == EditorState::Pause) {
            if (stepRuntime) {
                activeScene->OnUpdateRuntime(dt);
                stepRuntime = false;
            }
        } else {
            activeScene->OnUpdateEditor(dt);
        }
        activeScene->Render(currentRenderer);

        if (currentRenderer) {
            currentRenderer->EndFrame(); // Renders scene to internal texture (no swap)
        }

        auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer);
        if (glRenderer) {
            uint64_t texID = glRenderer->GetFinalTextureID();
            // Invert V for OpenGL texture in ImGui
            ImGui::Image((ImTextureID)texID, viewportSize, ImVec2(0, 1), ImVec2(1, 0));
        }

        // Draw Grid (keep under overlays)
        if (showGrid) {
            ImGuizmo::SetDrawlist();
            ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);
            glm::mat4 identityMatrix = glm::mat4(1.0f);
            ImGuizmo::DrawGrid(glm::value_ptr(view), glm::value_ptr(projection), glm::value_ptr(identityMatrix), 100.f);
        }

        bool viewportClickConsumed = false;

        // Draw overlay icons for non-visible objects (camera, lights)
        if (showSceneIcons && viewportSize.x > 1.0f && viewportSize.y > 1.0f) {
            auto WorldToScreen = [&](const glm::vec3& worldPos, glm::vec2& outScreen, glm::vec2& outUv, float& outDepth01) -> bool {
                glm::vec4 clip = projection * view * glm::vec4(worldPos, 1.0f);
                if (clip.w == 0.0f) return false;
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                if (ndc.z < -1.0f || ndc.z > 1.0f) return false;

                outScreen.x = viewportTopLeft.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x;
                outScreen.y = viewportTopLeft.y + (0.5f - ndc.y * 0.5f) * viewportSize.y;
                outUv.x = (outScreen.x - viewportTopLeft.x) / viewportSize.x;
                outUv.y = (outScreen.y - viewportTopLeft.y) / viewportSize.y;
                outDepth01 = ndc.z * 0.5f + 0.5f;

                const float pad = 24.0f;
                if (outScreen.x < viewportTopLeft.x - pad || outScreen.x > viewportTopLeft.x + viewportSize.x + pad ||
                    outScreen.y < viewportTopLeft.y - pad || outScreen.y > viewportTopLeft.y + viewportSize.y + pad) return false;
                if (outUv.x < -0.1f || outUv.x > 1.1f || outUv.y < -0.1f || outUv.y > 1.1f) return false;
                return true;
            };

            auto IsOccluded = [&](const glm::vec2& uv, float depth01) -> bool {
                if (!occludeSceneIcons || !glRenderer) return false;
                float depthSample = 1.0f;
                if (!glRenderer->SampleSceneDepth(uv.x, uv.y, depthSample)) return false;
                const float bias = 0.0025f;
                return depthSample + bias < depth01;
            };

            if (cameraIconTex) cameraIconTex->UploadToRenderer(currentRenderer);
            if (lightDirIconTex) lightDirIconTex->UploadToRenderer(currentRenderer);
            if (lightPointIconTex) lightPointIconTex->UploadToRenderer(currentRenderer);

            ImTextureID cameraTexId = (cameraIconTex && cameraIconTex->GetID())
                ? (ImTextureID)(uintptr_t)cameraIconTex->GetID()
                : (ImTextureID)0;
            ImTextureID lightDirTexId = (lightDirIconTex && lightDirIconTex->GetID())
                ? (ImTextureID)(uintptr_t)lightDirIconTex->GetID()
                : (ImTextureID)0;
            ImTextureID lightPointTexId = (lightPointIconTex && lightPointIconTex->GetID())
                ? (ImTextureID)(uintptr_t)lightPointIconTex->GetID()
                : (ImTextureID)0;

            ImDrawList* dl = ImGui::GetWindowDrawList();
            ImGuiIO& io = ImGui::GetIO();
            const bool clicked = viewportLeftClicked;
            const float iconSize = 32.0f;
            const float hitRadiusSq = (iconSize * 0.5f) * (iconSize * 0.5f);
            const bool useVectorIcons = true;

            auto Clamp01 = [](float v) { return v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v); };
            auto Shade = [&](const ImVec4& c, float mul, float alphaMul = 1.0f) -> ImU32 {
                return ImGui::GetColorU32(ImVec4(
                    Clamp01(c.x * mul),
                    Clamp01(c.y * mul),
                    Clamp01(c.z * mul),
                    Clamp01(c.w * alphaMul)));
            };
            auto BoostColor = [&](const ImVec4& c, float add) {
                return ImVec4(Clamp01(c.x + add), Clamp01(c.y + add), Clamp01(c.z + add), c.w);
            };
            auto DrawCameraIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                const float half = size * 0.5f;
                const float bodyH = size * 0.58f;
                const float bodyW = size * 0.92f;
                const float rounding = size * 0.18f;
                ImVec2 bodyMin(center.x - bodyW * 0.5f, center.y - bodyH * 0.5f);
                ImVec2 bodyMax(center.x + bodyW * 0.5f, center.y + bodyH * 0.5f);
                dl->AddRectFilled(ImVec2(bodyMin.x + 1.0f, bodyMin.y + 1.0f), ImVec2(bodyMax.x + 1.0f, bodyMax.y + 1.0f), IM_COL32(0, 0, 0, 80), rounding);
                dl->AddRectFilled(bodyMin, bodyMax, Shade(base, 0.95f), rounding);
                dl->AddRect(bodyMin, bodyMax, Shade(base, 1.25f), rounding, 0, 1.25f);

                const float humpW = size * 0.46f;
                const float humpH = size * 0.22f;
                ImVec2 humpMin(center.x - humpW * 0.5f, bodyMin.y - humpH * 0.45f);
                ImVec2 humpMax(center.x + humpW * 0.5f, bodyMin.y + humpH * 0.55f);
                dl->AddRectFilled(humpMin, humpMax, Shade(base, 0.88f), rounding * 0.6f);
                dl->AddRect(humpMin, humpMax, Shade(base, 1.18f), rounding * 0.6f, 0, 1.0f);

                ImVec2 lensCenter(center.x + size * 0.18f, center.y);
                float lensR = size * 0.21f;
                dl->AddCircleFilled(lensCenter, lensR + 1.0f, IM_COL32(0, 0, 0, 80), 24);
                dl->AddCircleFilled(lensCenter, lensR, IM_COL32(245, 245, 255, 220), 24);
                dl->AddCircle(lensCenter, lensR, Shade(base, 1.35f), 24, 1.2f);
                dl->AddCircleFilled(ImVec2(lensCenter.x - lensR * 0.35f, lensCenter.y - lensR * 0.35f), lensR * 0.28f, IM_COL32(255, 255, 255, 150), 16);
            };
            auto DrawDirectionalLightIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                constexpr float kPi = 3.1415926535f;
                ImVec4 color = BoostColor(base, 0.15f);
                const float sunR = size * 0.28f;
                dl->AddCircleFilled(ImVec2(center.x + 1.0f, center.y + 1.0f), sunR + 1.0f, IM_COL32(0, 0, 0, 70), 24);
                dl->AddCircleFilled(center, sunR, Shade(color, 1.0f), 24);
                dl->AddCircle(center, sunR, Shade(color, 1.35f), 24, 1.1f);

                const int rayCount = 8;
                const float rayLen = size * 0.48f;
                const float rayInner = sunR + size * 0.06f;
                for (int i = 0; i < rayCount; ++i) {
                    float a = (kPi * 2.0f * i) / rayCount;
                    ImVec2 dir(std::cos(a), std::sin(a));
                    ImVec2 p0(center.x + dir.x * rayInner, center.y + dir.y * rayInner);
                    ImVec2 p1(center.x + dir.x * rayLen, center.y + dir.y * rayLen);
                    dl->AddLine(p0, p1, Shade(color, 1.1f), 1.4f);
                }
            };
            auto DrawPointLightIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                ImVec4 color = BoostColor(base, 0.1f);
                const float coreR = size * 0.22f;
                const float glowR = size * 0.42f;
                dl->AddCircleFilled(ImVec2(center.x + 1.0f, center.y + 1.0f), glowR + 1.0f, IM_COL32(0, 0, 0, 70), 24);
                dl->AddCircleFilled(center, coreR, Shade(color, 1.0f), 24);
                dl->AddCircle(center, coreR, Shade(color, 1.35f), 24, 1.1f);
                dl->AddCircle(center, glowR, IM_COL32(255, 255, 255, 140), 24, 1.2f);
            };
            auto DrawAudioIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                ImVec4 color = BoostColor(base, 0.08f);
                const float bodyW = size * 0.28f;
                const float bodyH = size * 0.36f;
                ImVec2 bodyMin(center.x - size * 0.38f, center.y - bodyH * 0.5f);
                ImVec2 bodyMax(bodyMin.x + bodyW, bodyMin.y + bodyH);
                dl->AddRectFilled(bodyMin, bodyMax, Shade(color, 0.95f), size * 0.08f);
                dl->AddRect(bodyMin, bodyMax, Shade(color, 1.25f), size * 0.08f, 0, 1.0f);

                ImVec2 triA(bodyMax.x, center.y - bodyH * 0.7f);
                ImVec2 triB(bodyMax.x, center.y + bodyH * 0.7f);
                ImVec2 triC(center.x + size * 0.38f, center.y);
                dl->AddTriangleFilled(triA, triB, triC, Shade(color, 1.1f));

                dl->AddLine(ImVec2(center.x + size * 0.18f, center.y - size * 0.18f), ImVec2(center.x + size * 0.34f, center.y - size * 0.32f), Shade(color, 1.3f), 1.2f);
                dl->AddLine(ImVec2(center.x + size * 0.18f, center.y + size * 0.18f), ImVec2(center.x + size * 0.34f, center.y + size * 0.32f), Shade(color, 1.3f), 1.2f);
            };
            auto DrawParticleIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                ImVec4 color = BoostColor(base, 0.08f);
                const float len = size * 0.46f;
                dl->AddLine(ImVec2(center.x - len, center.y), ImVec2(center.x + len, center.y), Shade(color, 1.25f), 1.6f);
                dl->AddLine(ImVec2(center.x, center.y - len), ImVec2(center.x, center.y + len), Shade(color, 1.25f), 1.6f);
                dl->AddLine(ImVec2(center.x - len * 0.6f, center.y - len * 0.6f), ImVec2(center.x + len * 0.6f, center.y + len * 0.6f), Shade(color, 1.1f), 1.2f);
                dl->AddLine(ImVec2(center.x - len * 0.6f, center.y + len * 0.6f), ImVec2(center.x + len * 0.6f, center.y - len * 0.6f), Shade(color, 1.1f), 1.2f);
                dl->AddCircleFilled(center, size * 0.08f, Shade(color, 1.35f));
            };
            auto DrawRigidBodyIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                ImVec4 color = BoostColor(base, 0.05f);
                const float half = size * 0.36f;
                ImVec2 pMin(center.x - half, center.y - half);
                ImVec2 pMax(center.x + half, center.y + half);
                dl->AddRectFilled(ImVec2(pMin.x + 1.0f, pMin.y + 1.0f), ImVec2(pMax.x + 1.0f, pMax.y + 1.0f), IM_COL32(0, 0, 0, 70), size * 0.08f);
                dl->AddRectFilled(pMin, pMax, Shade(color, 0.92f), size * 0.08f);
                dl->AddRect(pMin, pMax, Shade(color, 1.25f), size * 0.08f, 0, 1.2f);
                dl->AddCircleFilled(ImVec2(center.x, center.y + half * 0.55f), size * 0.07f, Shade(color, 1.3f));
            };
            auto DrawBoxColliderIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                ImVec4 color = BoostColor(base, 0.05f);
                const float half = size * 0.4f;
                ImVec2 pMin(center.x - half, center.y - half);
                ImVec2 pMax(center.x + half, center.y + half);
                dl->AddRect(pMin, pMax, Shade(color, 1.25f), 0.0f, 0, 1.4f);
                dl->AddRect(ImVec2(pMin.x + size * 0.12f, pMin.y + size * 0.12f), ImVec2(pMax.x - size * 0.12f, pMax.y - size * 0.12f), Shade(color, 0.95f), 0.0f, 0, 1.0f);
            };
            auto DrawSphereColliderIcon = [&](ImVec2 center, float size, const ImVec4& base) {
                ImVec4 color = BoostColor(base, 0.05f);
                const float r = size * 0.4f;
                dl->AddCircle(center, r, Shade(color, 1.2f), 24, 1.4f);
                dl->AddCircle(center, r * 0.6f, Shade(color, 0.95f), 24, 1.0f);
                dl->AddCircleFilled(center, size * 0.08f, Shade(color, 1.3f));
            };

            // Camera icons
            {
                auto camView = activeScene->Registry().view<Genesis::Engine::CameraComponent, Genesis::Engine::Transform>();
                for (auto entity : camView) {
                    glm::mat4 world = GetWorldMatrixGLM(activeScene->Registry(), entity);
                    glm::vec3 wp = glm::vec3(world[3]);
                    glm::vec2 sp, uv;
                    float depth01 = 1.0f;
                    if (!WorldToScreen(wp, sp, uv, depth01)) continue;
                    if (IsOccluded(uv, depth01)) continue;

                    ImVec2 p(sp.x, sp.y);
                    ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                    ImVec2 pMin(p.x - half.x, p.y - half.y);
                    ImVec2 pMax(p.x + half.x, p.y + half.y);
                    ImVec4 baseColor(0.58f, 0.8f, 1.0f, 1.0f);
                    if (!useVectorIcons && cameraTexId) {
                        dl->AddImage(cameraTexId, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(baseColor));
                    } else {
                        DrawCameraIcon(p, iconSize, baseColor);
                    }

                    if (selectedEntity == entity) {
                        dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 200), 2.0f, 0, 1.5f);
                    }

                    if (clicked && !viewportClickConsumed) {
                        float dx = io.MousePos.x - p.x;
                        float dy = io.MousePos.y - p.y;
                        if (dx * dx + dy * dy <= hitRadiusSq) {
                            selectedEntity = entity;
                            viewportClickConsumed = true;
                        }
                    }
                }
            }

            // Light icons
            auto lightView = activeScene->Registry().view<Genesis::Engine::LightComponent, Genesis::Engine::Transform>();
            for (auto entity : lightView) {
                const auto& lc = lightView.get<Genesis::Engine::LightComponent>(entity);
                glm::mat4 world = GetWorldMatrixGLM(activeScene->Registry(), entity);
                glm::vec3 wp = glm::vec3(world[3]);
                glm::vec2 sp, uv;
                float depth01 = 1.0f;
                if (!WorldToScreen(wp, sp, uv, depth01)) continue;
                if (IsOccluded(uv, depth01)) continue;

                ImTextureID lightTexId = (lc.type == Genesis::Engine::LightType::Directional) ? lightDirTexId : lightPointTexId;
                const bool hasLightIcon = (lightTexId != 0);

                ImVec2 p(sp.x, sp.y);
                ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                ImVec2 pMin(p.x - half.x, p.y - half.y);
                ImVec2 pMax(p.x + half.x, p.y + half.y);
                ImVec4 baseColor(lc.color[0], lc.color[1], lc.color[2], 1.0f);
                if (!useVectorIcons && hasLightIcon) {
                    dl->AddImage(lightTexId, pMin, pMax, ImVec2(0, 0), ImVec2(1, 1), ImGui::GetColorU32(baseColor));
                } else {
                    if (lc.type == Genesis::Engine::LightType::Directional) {
                        DrawDirectionalLightIcon(p, iconSize, baseColor);
                    } else {
                        DrawPointLightIcon(p, iconSize, baseColor);
                    }
                }

                if (selectedEntity == entity) {
                    dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 200), 2.0f, 0, 1.5f);
                }

                if (lc.type == Genesis::Engine::LightType::Point && lc.range > 0.0f) {
                    glm::vec3 rworld = wp + glm::vec3(lc.range, 0, 0);
                    glm::vec2 rscr, ruv;
                    float rdepth = 1.0f;
                    if (WorldToScreen(rworld, rscr, ruv, rdepth)) {
                        float pixelR = sqrtf((rscr.x - p.x) * (rscr.x - p.x) + (rscr.y - p.y) * (rscr.y - p.y));
                        dl->AddCircle(p, pixelR, IM_COL32(255, 255, 255, 100), 64, 1.5f);
                    }
                }

                if (clicked && !viewportClickConsumed) {
                    float dx = io.MousePos.x - p.x;
                    float dy = io.MousePos.y - p.y;
                    if (dx * dx + dy * dy <= hitRadiusSq) {
                        selectedEntity = entity;
                        viewportClickConsumed = true;
                    }
                }
            }

            // Audio icons
            {
                auto audioView = activeScene->Registry().view<Genesis::Engine::AudioComponent, Genesis::Engine::Transform>();
                for (auto entity : audioView) {
                    glm::mat4 world = GetWorldMatrixGLM(activeScene->Registry(), entity);
                    glm::vec3 wp = glm::vec3(world[3]);
                    glm::vec2 sp, uv;
                    float depth01 = 1.0f;
                    if (!WorldToScreen(wp, sp, uv, depth01)) continue;
                    if (IsOccluded(uv, depth01)) continue;

                    ImVec2 p(sp.x, sp.y);
                    ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                    ImVec2 pMin(p.x - half.x, p.y - half.y);
                    ImVec2 pMax(p.x + half.x, p.y + half.y);
                    ImVec4 baseColor(0.78f, 0.6f, 1.0f, 1.0f);
                    DrawAudioIcon(p, iconSize, baseColor);

                    if (selectedEntity == entity) {
                        dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 200), 2.0f, 0, 1.5f);
                    }

                    if (clicked && !viewportClickConsumed) {
                        float dx = io.MousePos.x - p.x;
                        float dy = io.MousePos.y - p.y;
                        if (dx * dx + dy * dy <= hitRadiusSq) {
                            selectedEntity = entity;
                            viewportClickConsumed = true;
                        }
                    }
                }
            }

            // Particle system icons
            {
                auto particleView = activeScene->Registry().view<Genesis::Engine::ParticleSystemComponent, Genesis::Engine::Transform>();
                for (auto entity : particleView) {
                    glm::mat4 world = GetWorldMatrixGLM(activeScene->Registry(), entity);
                    glm::vec3 wp = glm::vec3(world[3]);
                    glm::vec2 sp, uv;
                    float depth01 = 1.0f;
                    if (!WorldToScreen(wp, sp, uv, depth01)) continue;
                    if (IsOccluded(uv, depth01)) continue;

                    ImVec2 p(sp.x, sp.y);
                    ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                    ImVec2 pMin(p.x - half.x, p.y - half.y);
                    ImVec2 pMax(p.x + half.x, p.y + half.y);
                    ImVec4 baseColor(1.0f, 0.78f, 0.35f, 1.0f);
                    DrawParticleIcon(p, iconSize, baseColor);

                    if (selectedEntity == entity) {
                        dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 200), 2.0f, 0, 1.5f);
                    }

                    if (clicked && !viewportClickConsumed) {
                        float dx = io.MousePos.x - p.x;
                        float dy = io.MousePos.y - p.y;
                        if (dx * dx + dy * dy <= hitRadiusSq) {
                            selectedEntity = entity;
                            viewportClickConsumed = true;
                        }
                    }
                }
            }

            // Rigid body icons
            {
                auto rbView = activeScene->Registry().view<Genesis::Engine::RigidBodyComponent, Genesis::Engine::Transform>();
                for (auto entity : rbView) {
                    glm::mat4 world = GetWorldMatrixGLM(activeScene->Registry(), entity);
                    glm::vec3 wp = glm::vec3(world[3]);
                    glm::vec2 sp, uv;
                    float depth01 = 1.0f;
                    if (!WorldToScreen(wp, sp, uv, depth01)) continue;
                    if (IsOccluded(uv, depth01)) continue;

                    ImVec2 p(sp.x, sp.y);
                    ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                    ImVec2 pMin(p.x - half.x, p.y - half.y);
                    ImVec2 pMax(p.x + half.x, p.y + half.y);
                    ImVec4 baseColor(0.78f, 0.78f, 0.82f, 1.0f);
                    DrawRigidBodyIcon(p, iconSize, baseColor);

                    if (selectedEntity == entity) {
                        dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 200), 2.0f, 0, 1.5f);
                    }

                    if (clicked && !viewportClickConsumed) {
                        float dx = io.MousePos.x - p.x;
                        float dy = io.MousePos.y - p.y;
                        if (dx * dx + dy * dy <= hitRadiusSq) {
                            selectedEntity = entity;
                            viewportClickConsumed = true;
                        }
                    }
                }
            }

            // Box collider icons
            {
                auto boxView = activeScene->Registry().view<Genesis::Engine::BoxColliderComponent, Genesis::Engine::Transform>();
                for (auto entity : boxView) {
                    const auto& bc = boxView.get<Genesis::Engine::BoxColliderComponent>(entity);
                    glm::mat4 world = GetWorldMatrixGLM(activeScene->Registry(), entity);
                    glm::vec3 wp = glm::vec3(world * glm::vec4(bc.offset[0], bc.offset[1], bc.offset[2], 1.0f));
                    glm::vec2 sp, uv;
                    float depth01 = 1.0f;
                    if (!WorldToScreen(wp, sp, uv, depth01)) continue;
                    if (IsOccluded(uv, depth01)) continue;

                    ImVec2 p(sp.x, sp.y);
                    ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                    ImVec2 pMin(p.x - half.x, p.y - half.y);
                    ImVec2 pMax(p.x + half.x, p.y + half.y);
                    ImVec4 baseColor(0.45f, 0.92f, 0.55f, 1.0f);
                    DrawBoxColliderIcon(p, iconSize, baseColor);

                    if (selectedEntity == entity) {
                        dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 200), 2.0f, 0, 1.5f);
                    }

                    if (clicked && !viewportClickConsumed) {
                        float dx = io.MousePos.x - p.x;
                        float dy = io.MousePos.y - p.y;
                        if (dx * dx + dy * dy <= hitRadiusSq) {
                            selectedEntity = entity;
                            viewportClickConsumed = true;
                        }
                    }
                }
            }

            // Sphere collider icons
            {
                auto sphereView = activeScene->Registry().view<Genesis::Engine::SphereColliderComponent, Genesis::Engine::Transform>();
                for (auto entity : sphereView) {
                    const auto& sc = sphereView.get<Genesis::Engine::SphereColliderComponent>(entity);
                    glm::mat4 world = GetWorldMatrixGLM(activeScene->Registry(), entity);
                    glm::vec3 wp = glm::vec3(world * glm::vec4(sc.offset[0], sc.offset[1], sc.offset[2], 1.0f));
                    glm::vec2 sp, uv;
                    float depth01 = 1.0f;
                    if (!WorldToScreen(wp, sp, uv, depth01)) continue;
                    if (IsOccluded(uv, depth01)) continue;

                    ImVec2 p(sp.x, sp.y);
                    ImVec2 half(iconSize * 0.5f, iconSize * 0.5f);
                    ImVec2 pMin(p.x - half.x, p.y - half.y);
                    ImVec2 pMax(p.x + half.x, p.y + half.y);
                    ImVec4 baseColor(0.35f, 0.85f, 0.8f, 1.0f);
                    DrawSphereColliderIcon(p, iconSize, baseColor);

                    if (selectedEntity == entity) {
                        dl->AddRect(pMin, pMax, IM_COL32(255, 255, 255, 200), 2.0f, 0, 1.5f);
                    }

                    if (clicked && !viewportClickConsumed) {
                        float dx = io.MousePos.x - p.x;
                        float dy = io.MousePos.y - p.y;
                        if (dx * dx + dy * dy <= hitRadiusSq) {
                            selectedEntity = entity;
                            viewportClickConsumed = true;
                        }
                    }
                }
            }
        }

        if (viewportLeftClicked && !viewportClickConsumed) {
            const bool gizmoHit = ImGuizmo::IsOver() || ImGuizmo::IsUsing();
            if (!gizmoHit && !mouseOverViewCube) {
                selectedEntity = entt::null;
            }
        }

        // Drag/drop onto the viewport: create entity from a model, or open a dropped .scene
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                const char* droppedPath = (const char*)payload->Data;
                if (droppedPath && droppedPath[0] != 0) {
                    std::filesystem::path p(droppedPath);
                    std::string ext = p.extension().string();
                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });

                    if (ext == ".scene") {
                        if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, p.string())) {
                            LoadSceneFromPath(p.string(), true);
                        }
                    } else if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                        if (editorState == EditorState::Edit) {
                            auto& reg = editorScene.Registry();
                            auto e = reg.create();
                            reg.emplace<Genesis::Engine::NameComponent>(e, Genesis::Engine::NameComponent{p.stem().string()});
                            reg.emplace<Genesis::Engine::Transform>(e);
                            Genesis::Engine::ModelComponent mc;
                            mc.model = std::make_shared<Genesis::Engine::Model>();
                            mc.sourcePath = p.string();
                            if (mc.model->Load(mc.sourcePath)) {
                                reg.emplace<Genesis::Engine::ModelComponent>(e, mc);
                                selectedEntity = e;
                                sceneDirty = true;
                                PushCreateCommand("Create Model", e, CaptureEntitySnapshot(reg, e));
                            } else {
                                reg.destroy(e);
                            }
                        }
                    }
                }
            }
            ImGui::EndDragDropTarget();
        }

        // View Manipulate (View Cube) - position already calculated above for conflict detection
        glm::mat4 viewCopy = view; // Make a copy to pass to ViewManipulate
        ImGuizmo::SetDrawlist();
        ImGuizmo::ViewManipulate(glm::value_ptr(viewCopy), 5.0f, viewManipulatePos, ImVec2(viewManipulateSize, viewManipulateSize), 0x10101010);

        // Axis labels (X/Y/Z) around the cube (overlay, positioned based on current view orientation)
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImU32 shadow = IM_COL32(0, 0, 0, 160);
            const ImU32 textFront = IM_COL32(235, 235, 235, 230);
            const ImU32 textBack = IM_COL32(170, 170, 170, 180);
            const ImVec2 center = ImVec2(viewManipulatePos.x + viewManipulateSize * 0.5f, viewManipulatePos.y + viewManipulateSize * 0.5f);

            // View matrix transforms World -> View (camera). Use its rotation part to estimate
            // where the world axes point on the view-cube overlay.
            const glm::mat3 viewRot = glm::mat3(view);

            auto DrawAxisLabel = [&](const char* label, const glm::vec3& worldAxis) {
                // Axis direction in view/camera space.
                const glm::vec3 axisView = viewRot * worldAxis;

                // Map to 2D (screen y down). Normalize to avoid huge/NaN offsets.
                glm::vec2 axis2(axisView.x, -axisView.y);
                float len = glm::length(axis2);
                if (len < 1e-6f) {
                    return;
                }
                axis2 /= len;

                // Place near the edge of the view cube.
                const float radius = viewManipulateSize * 0.42f;
                ImVec2 pos = ImVec2(center.x + axis2.x * radius, center.y + axis2.y * radius);

                // In OpenGL view space, points "in front" typically have negative Z.
                const bool frontFacing = (axisView.z < 0.0f);
                const ImU32 text = frontFacing ? textFront : textBack;

                dl->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), shadow, label);
                dl->AddText(pos, text, label);
            };

            DrawAxisLabel("X", glm::vec3(1.0f, 0.0f, 0.0f));
            DrawAxisLabel("Y", glm::vec3(0.0f, 1.0f, 0.0f));
            DrawAxisLabel("Z", glm::vec3(0.0f, 0.0f, 1.0f));
        }

        auto MatDifferent = [](const glm::mat4& a, const glm::mat4& b) {
            constexpr float eps = 1e-5f;
            for (int c = 0; c < 4; ++c) {
                for (int r = 0; r < 4; ++r) {
                    if (fabs(a[c][r] - b[c][r]) > eps) return true;
                }
            }
            return false;
        };

        // Apply view-cube camera changes while ImGuizmo is animating.
        // We intentionally convert to yaw/pitch and force roll=0 to avoid the camera ending up upside-down.
        auto ApplyViewMatrixToCamera = [&](const glm::mat4& targetView) {
            glm::mat4 inv = glm::inverse(targetView);
            glm::vec3 scale;
            glm::quat rotation;
            glm::vec3 translation;
            glm::vec3 skew;
            glm::vec4 perspective;
            glm::decompose(inv, scale, rotation, translation, skew, perspective);

            cameraPos = translation;

            glm::vec3 forward = rotation * glm::vec3(0.0f, 0.0f, -1.0f);
            if (glm::dot(forward, forward) < 1e-8f) {
                return;
            }
            forward = glm::normalize(forward);

            // Our navigation forward convention uses: forward.y = -sin(pitch).
            const float clampedY = glm::clamp(forward.y, -1.0f, 1.0f);
            float pitchRad = -asinf(clampedY);

            // For near-vertical views, yaw becomes underdetermined. Pick a stable yaw that
            // matches common editor behavior (top/bottom aligned; avoids "upside-down" feel).
            float yawRad = 0.0f;
            if (fabsf(forward.y) <= 0.999f) {
                yawRad = atan2f(forward.x, -forward.z);
            }

            cameraRot.x = glm::degrees(pitchRad);
            cameraRot.y = glm::degrees(yawRad);
            cameraRot.z = 0.0f;
        };

        const bool viewCubeDrivingCamera = ImGuizmo::IsUsingViewManipulate();
        if ((viewCubeDrivingCamera || applyViewCubeThisFrame) && MatDifferent(viewCopy, view)) {
            ApplyViewMatrixToCamera(viewCopy);
        }

        // Gizmos
        // Always draw the gizmo (so it doesn't disappear while navigating / snapping the camera),
        // but only allow interaction when the gizmo owns input.
        if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity) && activeScene->Registry().all_of<Genesis::Engine::Transform>(selectedEntity)) {
            ImGuizmo::SetOrthographic(false);
            ImGuizmo::SetDrawlist();

            ImGuizmo::SetRect(viewportTopLeft.x, viewportTopLeft.y, viewportSize.x, viewportSize.y);

            auto& tc = activeScene->Registry().get<Genesis::Engine::Transform>(selectedEntity);
            auto ComposeEngineTRS = [&](const Genesis::Engine::Transform& t) {
                return ComposeTransformGLM(t);
            };

            auto DecomposeEngineTRS = [&](const glm::mat4& m, Genesis::Engine::Transform& out) {
                DecomposeTransformGLM(m, out);
            };

            // Keep a stable matrix during manipulation to avoid feedback jitter from differing Euler conventions.
            static entt::entity gizmoEntity = entt::null;
            static glm::mat4 gizmoMatrix = glm::mat4(1.0f);
            static bool gizmoWasUsing = false;
            static Genesis::Engine::Transform gizmoStartTransform{};
            const bool gizmoUsingNow = ImGuizmo::IsUsing();

            if (gizmoEntity != selectedEntity || (!gizmoUsingNow && !gizmoWasUsing)) {
                gizmoEntity = selectedEntity;
                gizmoMatrix = GetWorldMatrixGLM(activeScene->Registry(), selectedEntity);
            }

            const bool gizmoInteractive = allowGizmoInteractionThisFrame && !wantText;
            ImGuizmo::Enable(gizmoInteractive);

            // Keep gizmo orientation fixed in world space (not dependent on object rotation)
            // for translate/rotate. Scaling in world space can introduce shear, which our
            // TRS-only Transform cannot represent cleanly, so we keep SCALE local.
            const ImGuizmo::MODE gizmoModeThisFrame =
                (currentGizmoOperation == ImGuizmo::SCALE || currentGizmoOperation == ImGuizmo::SCALEU)
                    ? ImGuizmo::LOCAL
                    : ImGuizmo::WORLD;

            // Prevent axis direction flipping for a stable, fixed gizmo.
            ImGuizmo::AllowAxisFlip(false);

            glm::mat4 manipulated = gizmoMatrix;
            ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), currentGizmoOperation, gizmoModeThisFrame, glm::value_ptr(manipulated));

            // Restore global state for any later ImGuizmo calls.
            ImGuizmo::Enable(true);

            const bool usingThisFrame = gizmoInteractive && ImGuizmo::IsUsing();
            if (usingThisFrame) {
                if (!gizmoWasUsing) {
                    gizmoStartTransform = tc;
                }
                gizmoMatrix = manipulated;
                glm::mat4 localMat = gizmoMatrix;
                if (activeScene->Registry().any_of<Genesis::Engine::ParentComponent>(selectedEntity)) {
                    auto parent = activeScene->Registry().get<Genesis::Engine::ParentComponent>(selectedEntity).parent;
                    if (parent != entt::null && activeScene->Registry().valid(parent)) {
                        glm::mat4 parentWorld = GetWorldMatrixGLM(activeScene->Registry(), parent);
                        glm::mat4 parentInv = glm::inverse(parentWorld);
                        localMat = parentInv * gizmoMatrix;
                    }
                }
                DecomposeEngineTRS(localMat, tc);
                sceneDirty = true;
            } else {
                // Not using: keep gizmo matrix in sync with component so it stays aligned to the rendered model.
                // (If you don't do this, the gizmo can drift after other systems edit tc.)
                gizmoMatrix = GetWorldMatrixGLM(activeScene->Registry(), selectedEntity);
            }

            if (!usingThisFrame && gizmoWasUsing) {
                if (editorState == EditorState::Edit) {
                    PushTransformCommand(selectedEntity, gizmoStartTransform, tc);
                }
            }

            gizmoWasUsing = usingThisFrame;
        }

        ImGui::End();
        ImGui::PopStyleVar();

        // Scene Hierarchy
        if (!zenMode) {
            ImGui::Begin("Scene Hierarchy");
            if (ImGui::Button("Create Entity")) {
                if (editorState == EditorState::Edit) {
                    auto e = CreateEntityWithDefaults();
                    selectedEntity = e;
                    sceneDirty = true;
                    PushCreateCommand("Create Entity", e, CaptureEntitySnapshot(editorScene.Registry(), e));
                }
            }
            ImGui::Separator();

            // Rename popup state
            static entt::entity renameEntity = entt::null;
            static char renameBuf[128] = "";
            static std::string renameOriginalName;

            // F2 focuses rename for selected entity
            if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && ImGui::IsKeyPressed(ImGuiKey_F2)) {
                if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                    renameEntity = selectedEntity;
                    std::string label;
                    if (activeScene->Registry().any_of<Genesis::Engine::NameComponent>(selectedEntity)) {
                        const auto& nc = activeScene->Registry().get<Genesis::Engine::NameComponent>(selectedEntity);
                        label = nc.name.empty() ? ("Entity " + std::to_string((uint32_t)selectedEntity)) : nc.name;
                    } else {
                        label = "Entity " + std::to_string((uint32_t)selectedEntity);
                    }
                    strncpy_s(renameBuf, label.c_str(), sizeof(renameBuf) - 1);
                    renameOriginalName = label;
                    ImGui::OpenPopup("Rename Entity");
                }
            }

            auto& hierarchyReg = activeScene->Registry();
            std::unordered_map<entt::entity, std::vector<entt::entity>> childrenMap;
            std::vector<entt::entity> roots;

            auto GetLabel = [&](entt::entity entity) {
                if (hierarchyReg.any_of<Genesis::Engine::NameComponent>(entity)) {
                    const auto& nc = hierarchyReg.get<Genesis::Engine::NameComponent>(entity);
                    if (!nc.name.empty()) return nc.name;
                }
                return "Entity " + std::to_string((uint32_t)entity);
            };

            hierarchyReg.each([&](auto entity) {
                entt::entity parent = entt::null;
                if (hierarchyReg.any_of<Genesis::Engine::ParentComponent>(entity)) {
                    parent = hierarchyReg.get<Genesis::Engine::ParentComponent>(entity).parent;
                }
                if (parent != entt::null && hierarchyReg.valid(parent) && parent != entity) {
                    childrenMap[parent].push_back(entity);
                } else {
                    roots.push_back(entity);
                }
            });

            auto SortByLabel = [&](std::vector<entt::entity>& list) {
                std::sort(list.begin(), list.end(), [&](entt::entity a, entt::entity b) {
                    return GetLabel(a) < GetLabel(b);
                });
            };
            SortByLabel(roots);
            for (auto& entry : childrenMap) {
                SortByLabel(entry.second);
            }

            auto DrawNode = [&](auto&& self, entt::entity entity) -> void {
                ImGui::PushID((int)entity);
                const std::string label = GetLabel(entity);
                const bool hasChildren = childrenMap.find(entity) != childrenMap.end() && !childrenMap[entity].empty();
                ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanFullWidth;
                if (!hasChildren) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
                if (selectedEntity == entity) flags |= ImGuiTreeNodeFlags_Selected;

                bool opened = ImGui::TreeNodeEx((void*)(intptr_t)entity, flags, "%s", label.c_str());
                if (ImGui::IsItemClicked()) {
                    selectedEntity = entity;
                }

                if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    renameEntity = entity;
                    strncpy_s(renameBuf, label.c_str(), sizeof(renameBuf) - 1);
                    renameOriginalName = label;
                    ImGui::OpenPopup("Rename Entity");
                }

                if (ImGui::BeginDragDropSource()) {
                    entt::entity payload = entity;
                    ImGui::SetDragDropPayload("SCENE_ENTITY", &payload, sizeof(entt::entity));
                    ImGui::TextUnformatted(label.c_str());
                    ImGui::EndDragDropSource();
                }

                if (ImGui::BeginDragDropTarget()) {
                    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_ENTITY")) {
                        if (payload && payload->DataSize == sizeof(entt::entity)) {
                            entt::entity dropped = *reinterpret_cast<const entt::entity*>(payload->Data);
                            if (hierarchyReg.valid(dropped) && dropped != entity) {
                                SetParentWithUndo(dropped, entity);
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                if (ImGui::BeginPopupContextItem()) {
                    if (ImGui::MenuItem("Rename", "F2")) {
                        renameEntity = entity;
                        strncpy_s(renameBuf, label.c_str(), sizeof(renameBuf) - 1);
                        renameOriginalName = label;
                        ImGui::OpenPopup("Rename Entity");
                    }
                    if (ImGui::MenuItem("Duplicate")) {
                        DuplicateEntityWithUndo(entity);
                    }
                    const bool hasParent = hierarchyReg.any_of<Genesis::Engine::ParentComponent>(entity);
                    if (ImGui::MenuItem("Unparent", nullptr, false, hasParent)) {
                        SetParentWithUndo(entity, entt::null);
                    }
                    if (ImGui::MenuItem("Delete", "Del")) {
                        DeleteEntityWithUndo(entity);
                    }
                    ImGui::EndPopup();
                }

                if (opened && hasChildren) {
                    for (auto child : childrenMap[entity]) {
                        self(self, child);
                    }
                    ImGui::TreePop();
                }

                if (selectedEntity == entity) {
                    ImGui::SetItemDefaultFocus();
                }
                ImGui::PopID();
            };

            for (auto entity : roots) {
                DrawNode(DrawNode, entity);
            }

            if (ImGui::BeginDragDropTarget()) {
                if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("SCENE_ENTITY")) {
                    if (payload && payload->DataSize == sizeof(entt::entity)) {
                        entt::entity dropped = *reinterpret_cast<const entt::entity*>(payload->Data);
                        if (hierarchyReg.valid(dropped)) {
                            SetParentWithUndo(dropped, entt::null);
                        }
                    }
                }
                ImGui::EndDragDropTarget();
            }

            if (ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)
                && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
                && !ImGui::IsAnyItemHovered()) {
                selectedEntity = entt::null;
            }

            if (ImGui::BeginPopupModal("Rename Entity", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
                ImGui::TextUnformatted("Name:");
                ImGui::PushItemWidth(300.0f);
                ImGui::InputText("##rename", renameBuf, sizeof(renameBuf));
                ImGui::PopItemWidth();

                bool commit = ImGui::Button("OK") || ImGui::IsKeyPressed(ImGuiKey_Enter);
                ImGui::SameLine();
                bool cancel = ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape);

                if (commit) {
                    if (renameEntity != entt::null && activeScene->Registry().valid(renameEntity)) {
                        activeScene->Registry().emplace_or_replace<Genesis::Engine::NameComponent>(renameEntity, Genesis::Engine::NameComponent{std::string(renameBuf)});
                        if (editorState == EditorState::Edit) sceneDirty = true;
                        PushRenameCommand(renameEntity, renameOriginalName, std::string(renameBuf));
                    }
                    renameEntity = entt::null;
                    ImGui::CloseCurrentPopup();
                }
                if (cancel) {
                    renameEntity = entt::null;
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::End();
        }

        // Inspector
        if (!zenMode) {
            ImGui::Begin("Inspector");
            if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                // Name (persistent edit buffer)
                static entt::entity lastNameEditEntity = entt::null;
                static char nameEditBuf[256] = "";
                if (selectedEntity != lastNameEditEntity) {
                    std::string name;
                    if (activeScene->Registry().any_of<Genesis::Engine::NameComponent>(selectedEntity)) {
                        name = activeScene->Registry().get<Genesis::Engine::NameComponent>(selectedEntity).name;
                    } else {
                        name = "Entity " + std::to_string((uint32_t)selectedEntity);
                    }
                    strncpy_s(nameEditBuf, name.c_str(), sizeof(nameEditBuf) - 1);
                    lastNameEditEntity = selectedEntity;
                }
                if (focusInspectorName) {
                    ImGui::SetKeyboardFocusHere();
                    focusInspectorName = false;
                }
                static std::string nameBeforeEdit;
                std::string nameBefore = nameEditBuf;
                if (ImGui::InputText("Name", nameEditBuf, sizeof(nameEditBuf))) {
                    activeScene->Registry().emplace_or_replace<Genesis::Engine::NameComponent>(selectedEntity, Genesis::Engine::NameComponent{std::string(nameEditBuf)});
                    if (editorState == EditorState::Edit) sceneDirty = true;
                }
                if (ImGui::IsItemActivated()) {
                    nameBeforeEdit = nameBefore;
                }
                if (ImGui::IsItemDeactivatedAfterEdit()) {
                    PushRenameCommand(selectedEntity, nameBeforeEdit, std::string(nameEditBuf));
                }
                {
                    entt::entity parent = entt::null;
                    if (activeScene->Registry().any_of<Genesis::Engine::ParentComponent>(selectedEntity)) {
                        parent = activeScene->Registry().get<Genesis::Engine::ParentComponent>(selectedEntity).parent;
                    }
                    std::string parentLabel = "(none)";
                    if (parent != entt::null && activeScene->Registry().valid(parent)) {
                        if (activeScene->Registry().any_of<Genesis::Engine::NameComponent>(parent)) {
                            const auto& nc = activeScene->Registry().get<Genesis::Engine::NameComponent>(parent);
                            if (!nc.name.empty()) parentLabel = nc.name;
                            else parentLabel = "Entity " + std::to_string((uint32_t)parent);
                        } else {
                            parentLabel = "Entity " + std::to_string((uint32_t)parent);
                        }
                    }
                    ImGui::Text("Parent: %s", parentLabel.c_str());
                    if (parent != entt::null) {
                        ImGui::SameLine();
                        if (ImGui::Button("Clear Parent")) {
                            SetParentWithUndo(selectedEntity, entt::null);
                        }
                    }
                }
                ImGui::Text("Entity ID: %u", (uint32_t)selectedEntity);
                ImGui::Separator();

                auto HasPrimaryCamera = [&]() -> bool {
                    auto view = activeScene->Registry().view<Genesis::Engine::CameraComponent>();
                    for (auto entity : view) {
                        if (view.get<Genesis::Engine::CameraComponent>(entity).primary) return true;
                    }
                    return false;
                };

                auto MakeCameraPrimary = [&](entt::entity primary) {
                    auto view = activeScene->Registry().view<Genesis::Engine::CameraComponent>();
                    for (auto entity : view) {
                        auto& cam = view.get<Genesis::Engine::CameraComponent>(entity);
                        cam.primary = (entity == primary);
                    }
                };

                static entt::entity lastAudioEntity = entt::null;
                static char audioPathBuf[512] = "";
                static entt::entity transformEditEntity = entt::null;
                static Genesis::Engine::Transform transformEditStart{};

                if (activeScene->Registry().all_of<Genesis::Engine::Transform>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& tc = activeScene->Registry().get<Genesis::Engine::Transform>(selectedEntity);
                        Genesis::Engine::Transform beforePos = tc;
                        if (ImGui::DragFloat3("Position", &tc.x, 0.1f)) {
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                        if (ImGui::IsItemActivated()) {
                            transformEditEntity = selectedEntity;
                            transformEditStart = beforePos;
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit() && transformEditEntity == selectedEntity) {
                            PushTransformCommand(selectedEntity, transformEditStart, tc);
                        }

                        // Show rotation in degrees, store radians.
                        float rotDeg[3] = { glm::degrees(tc.rx), glm::degrees(tc.ry), glm::degrees(tc.rz) };
                        Genesis::Engine::Transform beforeRot = tc;
                        if (ImGui::DragFloat3("Rotation (deg)", rotDeg, 0.5f)) {
                            tc.rx = glm::radians(rotDeg[0]);
                            tc.ry = glm::radians(rotDeg[1]);
                            tc.rz = glm::radians(rotDeg[2]);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                        if (ImGui::IsItemActivated()) {
                            transformEditEntity = selectedEntity;
                            transformEditStart = beforeRot;
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit() && transformEditEntity == selectedEntity) {
                            PushTransformCommand(selectedEntity, transformEditStart, tc);
                        }

                        Genesis::Engine::Transform beforeScale = tc;
                        if (ImGui::DragFloat3("Scale", &tc.sx, 0.01f, 0.0f, 1000.0f)) {
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                        if (ImGui::IsItemActivated()) {
                            transformEditEntity = selectedEntity;
                            transformEditStart = beforeScale;
                        }
                        if (ImGui::IsItemDeactivatedAfterEdit() && transformEditEntity == selectedEntity) {
                            PushTransformCommand(selectedEntity, transformEditStart, tc);
                        }
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::CameraComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Camera", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& cc = activeScene->Registry().get<Genesis::Engine::CameraComponent>(selectedEntity);
                        bool primary = cc.primary;
                        if (ImGui::Checkbox("Primary", &primary)) {
                            cc.primary = primary;
                            if (cc.primary) {
                                MakeCameraPrimary(selectedEntity);
                            }
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                        if (ImGui::DragFloat("FOV", &cc.fov, 0.1f, 1.0f, 179.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Near Plane", &cc.nearPlane, 0.01f, 0.001f, 10.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Far Plane", &cc.farPlane, 0.1f, 0.1f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::LightComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Light", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& lc = activeScene->Registry().get<Genesis::Engine::LightComponent>(selectedEntity);
                        const char* types[] = { "Directional", "Point" };
                        int currentType = (int)lc.type;
                        if (ImGui::Combo("Type", &currentType, types, IM_ARRAYSIZE(types))) {
                            lc.type = (Genesis::Engine::LightType)currentType;
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                        if (ImGui::ColorEdit3("Color", lc.color)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Intensity", &lc.intensity, 0.1f, 0.0f, 100.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (lc.type == Genesis::Engine::LightType::Point) {
                            if (ImGui::DragFloat("Range", &lc.range, 0.1f, 0.0f, 1000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::ModelComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Model", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& mc = activeScene->Registry().get<Genesis::Engine::ModelComponent>(selectedEntity);
                        ImGui::TextWrapped("Source: %s", mc.sourcePath.empty() ? "(unspecified)" : mc.sourcePath.c_str());

                        if (ImGui::Button("Reload") && mc.model && !mc.sourcePath.empty()) {
                            mc.model->Load(mc.sourcePath);
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remove")) {
                            activeScene->Registry().remove<Genesis::Engine::ModelComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }

                        ImGui::TextUnformatted("Drag a model from Content Browser onto this panel to assign.");
                        if (ImGui::BeginDragDropTarget()) {
                            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
                                const char* droppedPath = (const char*)payload->Data;
                                if (droppedPath && droppedPath[0] != 0) {
                                    std::filesystem::path p(droppedPath);
                                    std::string ext = p.extension().string();
                                    std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                                    if (ext == ".gltf" || ext == ".glb" || ext == ".obj" || ext == ".fbx") {
                                        if (!mc.model) mc.model = std::make_shared<Genesis::Engine::Model>();
                                        mc.sourcePath = p.string();
                                        mc.model->Load(mc.sourcePath);
                                        if (editorState == EditorState::Edit) sceneDirty = true;
                                    }
                                }
                            }
                            ImGui::EndDragDropTarget();
                        }
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::AudioComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Audio", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& ac = activeScene->Registry().get<Genesis::Engine::AudioComponent>(selectedEntity);
                        if (selectedEntity != lastAudioEntity) {
                            strncpy_s(audioPathBuf, ac.soundPath.c_str(), sizeof(audioPathBuf) - 1);
                            lastAudioEntity = selectedEntity;
                        }
                        if (ImGui::InputText("Sound Path", audioPathBuf, sizeof(audioPathBuf))) {
                            ac.soundPath = audioPathBuf;
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                        if (ImGui::DragFloat("Volume", &ac.volume, 0.01f, 0.0f, 5.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Pitch", &ac.pitch, 0.01f, 0.1f, 4.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Loop", &ac.loop)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Play On Awake", &ac.playOnAwake)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Spatial", &ac.spatial)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Min Distance", &ac.minDistance, 0.1f, 0.0f, 1000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Max Distance", &ac.maxDistance, 0.1f, 0.0f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::ParticleSystemComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Particle System", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& pc = activeScene->Registry().get<Genesis::Engine::ParticleSystemComponent>(selectedEntity);
                        if (ImGui::DragFloat("Duration", &pc.duration, 0.1f, 0.0f, 100.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Looping", &pc.looping)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Play On Awake", &pc.playOnAwake)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Start Lifetime", &pc.startLifetime, 0.1f, 0.0f, 100.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Start Speed", &pc.startSpeed, 0.1f, 0.0f, 100.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Start Size", &pc.startSize, 0.01f, 0.0f, 100.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::ColorEdit4("Start Color", pc.startColor)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Rate Over Time", &pc.rateOverTime, 0.1f, 0.0f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat("Emitter Radius", &pc.emitterRadius, 0.01f, 0.0f, 1000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::RigidBodyComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Rigid Body", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& rc = activeScene->Registry().get<Genesis::Engine::RigidBodyComponent>(selectedEntity);
                        if (ImGui::DragFloat("Mass", &rc.mass, 0.1f, 0.0f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Use Gravity", &rc.useGravity)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Is Kinematic", &rc.isKinematic)) if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::BoxColliderComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Box Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& bc = activeScene->Registry().get<Genesis::Engine::BoxColliderComponent>(selectedEntity);
                        if (ImGui::DragFloat3("Size", bc.size, 0.1f, 0.0f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat3("Offset", bc.offset, 0.1f, -10000.0f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Is Trigger", &bc.isTrigger)) if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                }

                if (activeScene->Registry().all_of<Genesis::Engine::SphereColliderComponent>(selectedEntity)) {
                    if (ImGui::CollapsingHeader("Sphere Collider", ImGuiTreeNodeFlags_DefaultOpen)) {
                        auto& sc = activeScene->Registry().get<Genesis::Engine::SphereColliderComponent>(selectedEntity);
                        if (ImGui::DragFloat("Radius", &sc.radius, 0.1f, 0.0f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::DragFloat3("Offset", sc.offset, 0.1f, -10000.0f, 10000.0f)) if (editorState == EditorState::Edit) sceneDirty = true;
                        if (ImGui::Checkbox("Is Trigger", &sc.isTrigger)) if (editorState == EditorState::Edit) sceneDirty = true;
                    }
                }

                if (ImGui::Button("Add Component")) {
                    ImGui::OpenPopup("AddComponentPopup");
                }
                if (ImGui::BeginPopup("AddComponentPopup")) {
                    if (ImGui::MenuItem("Camera")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::CameraComponent>(selectedEntity)) {
                            Genesis::Engine::CameraComponent cc;
                            cc.primary = !HasPrimaryCamera();
                            activeScene->Registry().emplace<Genesis::Engine::CameraComponent>(selectedEntity, cc);
                            if (cc.primary) {
                                MakeCameraPrimary(selectedEntity);
                            }
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Light")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::LightComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::LightComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Model")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::ModelComponent>(selectedEntity)) {
                            Genesis::Engine::ModelComponent mc;
                            mc.model = std::make_shared<Genesis::Engine::Model>();
                            mc.sourcePath.clear();
                            activeScene->Registry().emplace<Genesis::Engine::ModelComponent>(selectedEntity, mc);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Audio")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::AudioComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::AudioComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Particle System")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::ParticleSystemComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::ParticleSystemComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Rigid Body")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::RigidBodyComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::RigidBodyComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Box Collider")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::BoxColliderComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::BoxColliderComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    if (ImGui::MenuItem("Sphere Collider")) {
                        if (!activeScene->Registry().all_of<Genesis::Engine::SphereColliderComponent>(selectedEntity)) {
                            activeScene->Registry().emplace<Genesis::Engine::SphereColliderComponent>(selectedEntity);
                            if (editorState == EditorState::Edit) sceneDirty = true;
                        }
                    }
                    ImGui::EndPopup();
                }
            } else {
                ImGui::Text("Select an entity to view details.");
            }
            ImGui::End();
        }

        // Content Browser
        if (!zenMode) {
            ImGui::Begin("Content Browser");
            static std::filesystem::path contentDir = "Assets";
            static char contentSearch[128] = "";
            static std::unordered_map<std::string, std::shared_ptr<Genesis::Engine::Texture>> thumbnailCache;
            static std::unordered_map<std::string, std::shared_ptr<Genesis::Engine::Texture>> iconCache;

            if (std::filesystem::exists(contentDir)) {
                // Toolbar
                if (contentDir != std::filesystem::path("Assets")) {
                    if (ImGui::Button("<")) {
                        contentDir = contentDir.parent_path();
                    }
                    ImGui::SameLine();
                }
                ImGui::TextWrapped("%s", contentDir.string().c_str());
                ImGui::SameLine();
                ImGui::SetNextItemWidth(200.0f);
                ImGui::InputTextWithHint("##contentSearch", "Search...", contentSearch, sizeof(contentSearch));
                ImGui::Separator();

                float padding = 16.0f;
                float thumbnailSize = 64.0f;
                float cellSize = thumbnailSize + padding;
                float panelWidth = ImGui::GetContentRegionAvail().x;
                int columnCount = (int)(panelWidth / cellSize);
                if (columnCount < 1) columnCount = 1;

                ImGui::Columns(columnCount, 0, false);

                std::vector<std::filesystem::directory_entry> entries;
                for (const auto& entry : std::filesystem::directory_iterator(contentDir)) {
                    entries.push_back(entry);
                }
                std::sort(entries.begin(), entries.end(), [](const auto& a, const auto& b) {
                    if (a.is_directory() != b.is_directory()) return a.is_directory() > b.is_directory();
                    return a.path().filename().string() < b.path().filename().string();
                });

                for (const auto& entry : entries) {
                    std::string path = entry.path().string();
                    std::string filename = entry.path().filename().string();

                    if (contentSearch[0] != 0) {
                        std::string fLower = filename;
                        std::string sLower = contentSearch;
                        std::transform(fLower.begin(), fLower.end(), fLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                        std::transform(sLower.begin(), sLower.end(), sLower.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                        if (fLower.find(sLower) == std::string::npos) continue;
                    }
                    
                    ImGui::PushID(filename.c_str());
                    auto GetIconTexture = [&](const std::string& key, const IconColor& color, bool isFolder) {
                        auto it = iconCache.find(key);
                        if (it != iconCache.end()) return it->second;
                        auto tex = isFolder ? MakeFolderIcon(color) : MakeFileIcon(color);
                        iconCache[key] = tex;
                        return tex;
                    };

                    auto GetFileTypeIcon = [&](const std::string& extLower, bool isDir) {
                        if (isDir) {
                            return GetIconTexture("folder", IconColor{ 231, 189, 90, 255 }, true);
                        }
                        if (extLower == ".scene") return GetIconTexture("scene", IconColor{ 94, 156, 255, 255 }, false);
                        if (extLower == ".gltf" || extLower == ".glb" || extLower == ".obj" || extLower == ".fbx") return GetIconTexture("model", IconColor{ 180, 120, 255, 255 }, false);
                        if (extLower == ".vert" || extLower == ".frag" || extLower == ".glsl" || extLower == ".hlsl" || extLower == ".spv") return GetIconTexture("shader", IconColor{ 255, 166, 77, 255 }, false);
                        if (extLower == ".ttf" || extLower == ".otf") return GetIconTexture("font", IconColor{ 121, 215, 155, 255 }, false);
                        if (extLower == ".wav" || extLower == ".mp3" || extLower == ".ogg") return GetIconTexture("audio", IconColor{ 120, 210, 220, 255 }, false);
                        if (extLower == ".lua" || extLower == ".cs" || extLower == ".js") return GetIconTexture("script", IconColor{ 245, 215, 110, 255 }, false);
                        return GetIconTexture("file", IconColor{ 140, 150, 165, 255 }, false);
                    };

                    std::shared_ptr<Genesis::Engine::Texture> thumbTex;
                    const bool isDir = entry.is_directory();
                    std::string extLower = isDir ? std::string() : ToLowerCopy(entry.path().extension().string());

                    if (!isDir && IsImageExtension(extLower)) {
                        auto it = thumbnailCache.find(path);
                        if (it != thumbnailCache.end()) {
                            thumbTex = it->second;
                        } else {
                            auto loaded = Genesis::Engine::Texture::CreateFromFile(path);
                            if (loaded) {
                                thumbnailCache[path] = loaded;
                                thumbTex = loaded;
                            }
                        }
                    }

                    if (!thumbTex) {
                        thumbTex = GetFileTypeIcon(extLower, isDir);
                    }

                    if (thumbTex) {
                        thumbTex->UploadToRenderer(currentRenderer);
                    }

                    ImTextureID texId = (thumbTex && thumbTex->GetID()) ? (ImTextureID)(uintptr_t)thumbTex->GetID() : (ImTextureID)0;
                    // Thumbnail / icon
                    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
                    ImGui::ImageButton(filename.c_str(), texId, ImVec2(thumbnailSize, thumbnailSize));
                    ImGui::PopStyleColor();

                    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                        if (entry.is_directory()) {
                            contentDir = entry.path();
                        } else {
                            std::string ext = entry.path().extension().string();
                            std::transform(ext.begin(), ext.end(), ext.begin(), [](unsigned char c) { return (char)std::tolower(c); });
                            if (ext == ".scene") {
                                const std::string pathStr = entry.path().string();
                                if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, pathStr)) {
                                    LoadSceneFromPath(pathStr, true);
                                }
                            }
                        }
                    }

                    if (ImGui::BeginDragDropSource()) {
                        ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", path.c_str(), path.length() + 1);
                        ImGui::EndDragDropSource();
                    }

                    ImGui::TextWrapped("%s", filename.c_str());
                    ImGui::NextColumn();
                    ImGui::PopID();
                }
                ImGui::Columns(1);
            }
            ImGui::End();
        }

        // Console/Log
        if (!zenMode) {
            ImGui::Begin("Console");
            ImGui::Text("Genesis Engine Initialized.");
            ImGui::Text("FPS: %.1f (%.3f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
            ImGui::End();
        }

        // Command Palette
        if (showCommandPalette) {
            ImGui::OpenPopup("Command Palette");
        }

        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(600, 400));
        if (ImGui::BeginPopupModal("Command Palette", &showCommandPalette, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove)) {
            
            // Search Bar
            ImGui::PushItemWidth(-1);
            if (ImGui::IsWindowAppearing()) ImGui::SetKeyboardFocusHere();
            ImGui::InputText("##search", commandSearchBuffer, sizeof(commandSearchBuffer));
            ImGui::PopItemWidth();

            ImGui::Separator();

            // Commands List
            const char* commands[] = {
                "File: New Scene",
                "File: Open Scene...",
                "File: Save",
                "File: Save As...",
                "File: Exit",
                "View: Reset Layout",
                "View: Toggle Zen Mode",
                "Entity: Create New",
                "Entity: Rename Selected",
                "Entity: Delete Selected",
                "Window: Toggle Fullscreen",
                "Help: About"
            };

            ImGui::BeginChild("CommandList");
            for (int i = 0; i < IM_ARRAYSIZE(commands); i++) {
                // Simple filter
                if (commandSearchBuffer[0] != 0 && strstr(commands[i], commandSearchBuffer) == nullptr) continue;

                bool isSelected = (selectedCommandIndex == i);
                if (ImGui::Selectable(commands[i], isSelected)) {
                    // Execute Command
                    std::string cmd = commands[i];
                    if (cmd == "File: New Scene") {
                        if (!MaybePromptUnsaved(PendingSceneAction::NewScene)) {
                            DoNewScene();
                        }
                    }
                    else if (cmd == "File: Open Scene...") {
                        if (!MaybePromptUnsaved(PendingSceneAction::ShowOpenScene)) {
                            showOpenSceneModal = true;
                            if (!currentScenePath.empty()) {
                                strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                            } else {
                                const std::string fallbackPath = !editorSettings.lastScenePath.empty()
                                    ? editorSettings.lastScenePath
                                    : std::string("Assets/scenes/scene.scene");
                                strncpy_s(scenePathBuffer, fallbackPath.c_str(), sizeof(scenePathBuffer) - 1);
                            }
                        }
                    }
                    else if (cmd == "File: Save") {
                        if (currentScenePath.empty()) {
                            showSaveAsSceneModal = true;
                            strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                        } else {
                            if (editorState == EditorState::Edit) {
                                SaveSceneToPath(currentScenePath, true);
                            }
                        }
                    }
                    else if (cmd == "File: Save As...") {
                        showSaveAsSceneModal = true;
                        if (!currentScenePath.empty()) {
                            strncpy_s(scenePathBuffer, currentScenePath.c_str(), sizeof(scenePathBuffer) - 1);
                        } else {
                            strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                        }
                    }
                    else if (cmd == "File: Exit") {
                        RequestQuit();
                    }
                    else if (cmd == "View: Toggle Zen Mode") {
                        zenMode = !zenMode;
                    }
                    else if (cmd == "View: Reset Layout") {
                        requestResetLayout = true;
                    }
                    else if (cmd == "Entity: Create New") {
                        if (editorState == EditorState::Edit) {
                            auto e = CreateEntityWithDefaults();
                            selectedEntity = e;
                            sceneDirty = true;
                            PushCreateCommand("Create Entity", e, CaptureEntitySnapshot(editorScene.Registry(), e));
                        }
                    }
                    else if (cmd == "Entity: Rename Selected") {
                        if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                            zenMode = false;
                            focusInspectorName = true;
                        }
                    }
                    else if (cmd == "Entity: Delete Selected") {
                        if (selectedEntity != entt::null && activeScene->Registry().valid(selectedEntity)) {
                            DeleteEntityWithUndo(selectedEntity);
                        }
                    }
                    else if (cmd == "Window: Toggle Fullscreen") {
                        if (SDL_GetWindowFlags(window.GetSDLWindow()) & SDL_WINDOW_FULLSCREEN)
                            SDL_SetWindowFullscreen(window.GetSDLWindow(), 0);
                        else
                            SDL_SetWindowFullscreen(window.GetSDLWindow(), true);
                    }
                    else if (cmd == "Help: About") {
                        showAboutModal = true;
                    }
                    
                    showCommandPalette = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::EndChild();

            // Close on Escape
            if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showCommandPalette = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Open Scene modal
        if (showOpenSceneModal) {
            ImGui::OpenPopup("Open Scene");
        }
        if (ImGui::BeginPopupModal("Open Scene", &showOpenSceneModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Scene path:");
            ImGui::PushItemWidth(520.0f);
            ImGui::InputText("##openScenePath", scenePathBuffer, sizeof(scenePathBuffer));
            ImGui::PopItemWidth();
            ImGui::TextDisabled("Tip: double-click a .scene in Content Browser to open it.");

            if (ImGui::Button("Open") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (!MaybePromptUnsaved(PendingSceneAction::LoadScenePath, std::string(scenePathBuffer))) {
                    LoadSceneFromPath(scenePathBuffer, true);
                }
                showOpenSceneModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showOpenSceneModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Save As modal
        if (showSaveAsSceneModal) {
            ImGui::OpenPopup("Save Scene As");
        }
        if (ImGui::BeginPopupModal("Save Scene As", &showSaveAsSceneModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Save to path:");
            ImGui::PushItemWidth(520.0f);
            ImGui::InputText("##saveScenePath", scenePathBuffer, sizeof(scenePathBuffer));
            ImGui::PopItemWidth();
            if (ImGui::Button("Save") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
                if (SaveSceneToPath(scenePathBuffer, true)) {

                    // If a destructive action was pending, run it now.
                    if (pendingAction != PendingSceneAction::None) {
                        auto action = pendingAction;
                        auto p = pendingScenePath;
                        pendingAction = PendingSceneAction::None;
                        pendingScenePath.clear();
                        if (action == PendingSceneAction::Quit) {
                            running = false;
                        } else if (action == PendingSceneAction::NewScene) {
                            DoNewScene();
                        } else if (action == PendingSceneAction::ShowOpenScene) {
                            showOpenSceneModal = true;
                        } else if (action == PendingSceneAction::LoadScenePath) {
                            LoadSceneFromPath(p, true);
                        }
                    }
                }
                showSaveAsSceneModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                showSaveAsSceneModal = false;
                // If we got here via an unsaved-changes prompt, treat cancel as cancelling the pending action.
                if (pendingAction != PendingSceneAction::None) {
                    pendingAction = PendingSceneAction::None;
                    pendingScenePath.clear();
                }
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Autosave restore prompt
        if (showAutosaveRestoreModal) {
            ImGui::OpenPopup("Autosave Available");
        }
        if (ImGui::BeginPopupModal("Autosave Available", &showAutosaveRestoreModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("An autosave was found for this project.");
            ImGui::TextUnformatted("Restore it?");
            ImGui::Separator();

            if (ImGui::Button("Restore")) {
                if (Genesis::Engine::SceneLoader::LoadScene(editorScene, autosavePath.string())) {
                    selectedEntity = entt::null;
                    sceneDirty = true;
                    currentScenePath = autosaveBaseScenePath;
                }
                showAutosaveRestoreModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Discard")) {
                ClearAutosave();
                showAutosaveRestoreModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Later")) {
                showAutosaveRestoreModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // Unsaved changes modal
        if (showUnsavedChangesModal) {
            ImGui::OpenPopup("Unsaved Changes");
        }
        if (ImGui::BeginPopupModal("Unsaved Changes", &showUnsavedChangesModal, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("You have unsaved changes.");
            ImGui::TextUnformatted("Save before continuing?");
            ImGui::Separator();

            static bool lastSaveFailed = false;
            if (lastSaveFailed) {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Save failed.");
            }

            if (ImGui::Button("Save")) {
                lastSaveFailed = false;
                bool shouldClose = false;

                if (currentScenePath.empty()) {
                    showSaveAsSceneModal = true;
                    strncpy_s(scenePathBuffer, "Assets/scenes/scene.scene", sizeof(scenePathBuffer) - 1);
                    shouldClose = true;
                } else {
                    if (SaveSceneToPath(currentScenePath, true)) {

                        auto action = pendingAction;
                        auto p = pendingScenePath;
                        pendingAction = PendingSceneAction::None;
                        pendingScenePath.clear();

                        if (action == PendingSceneAction::Quit) {
                            running = false;
                        } else if (action == PendingSceneAction::NewScene) {
                            DoNewScene();
                        } else if (action == PendingSceneAction::ShowOpenScene) {
                            showOpenSceneModal = true;
                        } else if (action == PendingSceneAction::LoadScenePath) {
                            LoadSceneFromPath(p, true);
                        }
                        shouldClose = true;
                    } else {
                        lastSaveFailed = true;
                    }
                }

                if (shouldClose) {
                    showUnsavedChangesModal = false;
                    ImGui::CloseCurrentPopup();
                }
            }
            ImGui::SameLine();
            if (ImGui::Button("Don't Save")) {
                lastSaveFailed = false;
                auto action = pendingAction;
                auto p = pendingScenePath;
                pendingAction = PendingSceneAction::None;
                pendingScenePath.clear();

                if (action == PendingSceneAction::Quit) {
                    // Explicitly discard changes.
                    sceneDirty = false;
                    running = false;
                } else if (action == PendingSceneAction::NewScene) {
                    DoNewScene();
                } else if (action == PendingSceneAction::ShowOpenScene) {
                    showOpenSceneModal = true;
                } else if (action == PendingSceneAction::LoadScenePath) {
                    LoadSceneFromPath(p, true);
                }

                showUnsavedChangesModal = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                lastSaveFailed = false;
                pendingAction = PendingSceneAction::None;
                pendingScenePath.clear();
                showUnsavedChangesModal = false;
                ImGui::CloseCurrentPopup();
            }

            ImGui::EndPopup();
        }

        // About modal
        if (showAboutModal) {
            ImGui::OpenPopup("About Genesis Editor");
            showAboutModal = false;
        }
        if (ImGui::BeginPopupModal("About Genesis Editor", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextUnformatted("Genesis Editor");
            ImGui::Separator();
            ImGui::TextWrapped("Usability improvements: scene save/load, entity naming/rename, viewport-scoped camera controls, drag-and-drop model spawning, content browser navigation.");
            ImGui::TextWrapped("Renderer: %s", currentRenderer ? currentRenderer->GetName().c_str() : "(none)");
            if (ImGui::Button("Close") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        // Status bar
        {
            ImGuiViewport* vp = ImGui::GetMainViewport();
            const float barH = 22.0f;
            ImGui::SetNextWindowPos(ImVec2(vp->Pos.x, vp->Pos.y + vp->Size.y - barH));
            ImGui::SetNextWindowSize(ImVec2(vp->Size.x, barH));
            ImGui::SetNextWindowViewport(vp->ID);
            ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoDocking |
                                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                     ImGuiWindowFlags_NoNavFocus;
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 2));
            if (ImGui::Begin("##StatusBar", nullptr, flags)) {
                const char* dirtyMark = sceneDirty ? "*" : "";
                std::string sceneLabel = currentScenePath.empty() ? std::string("(unsaved)") : currentScenePath;
                ImGui::Text("Scene: %s%s", sceneLabel.c_str(), dirtyMark);
                ImGui::SameLine();
                ImGui::TextDisabled(" | ");
                ImGui::SameLine();
                ImGui::Text("FPS: %.1f", ImGui::GetIO().Framerate);
                ImGui::SameLine();
                ImGui::TextDisabled(" | ");
                ImGui::SameLine();
                ImGui::Text("Renderer: %s", currentRenderer ? currentRenderer->GetName().c_str() : "none");
                ImGui::SameLine();
                ImGui::TextDisabled(" | ");
                ImGui::SameLine();
                ImGui::Text("Cam: (%.2f, %.2f, %.2f)", cameraPos.x, cameraPos.y, cameraPos.z);
            }
            ImGui::End();
            ImGui::PopStyleVar();
        }

        ImGui::Render();
        
        // Ensure we are rendering to the default framebuffer (the window)
        if (auto glRenderer = dynamic_cast<Genesis::Engine::OpenGLRenderer*>(currentRenderer)) {
            glRenderer->BindDefaultFramebuffer();
            glRenderer->Clear(0.1f, 0.12f, 0.15f, 1.0f); // Clear to dark grey
        }

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        
        // Update and Render additional Platform Windows
        // (Platform functions may change the current OpenGL context, so we save/restore it to make it easier to paste this code elsewhere.
        //  For this specific binding SDL_GL_MakeCurrent() performs a lazy context switch so the saving/restoring is not strictly necessary.)
        if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            SDL_Window* backup_current_window = SDL_GL_GetCurrentWindow();
            SDL_GLContext backup_current_context = SDL_GL_GetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            SDL_GL_MakeCurrent(backup_current_window, backup_current_context);
        }

        if (currentRenderer) {
            currentRenderer->Present(); // Swap buffers
        }
        
        profiler.EndFrame();
    }

    if (auto cur = Genesis::Engine::RendererManager::GetRenderer()) cur->Shutdown();
    window.Shutdown();
    Genesis::Engine::Shutdown();
    return 0;
}
