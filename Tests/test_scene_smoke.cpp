#include "catch_amalgamated.hpp"
#include "engine/Scene.h"
#include "engine/SceneLoader.h"
#include "engine/Components.h"
#include "engine/UI.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

using namespace Genesis::Engine;
namespace fs = std::filesystem;
using Catch::Approx;

static fs::path MakeTempScenePath(const std::string& name) {
    fs::path dir = fs::temp_directory_path() / "genesis_scene_tests";
    std::error_code ec;
    fs::create_directories(dir, ec);
    return dir / name;
}

static std::string ReadFileText(const fs::path& path) {
    std::ifstream file(path, std::ios::binary);
    std::ostringstream buffer;
    buffer << file.rdbuf();
    return buffer.str();
}

static fs::path FindDefaultScenePath() {
    fs::path current = fs::current_path();
    for (int i = 0; i < 8; ++i) {
        fs::path candidate = current / "Assets" / "scenes" / "default.scene";
        if (fs::exists(candidate)) {
            return candidate;
        }
        if (!current.has_parent_path()) break;
        current = current.parent_path();
    }
    return {};
}

TEST_CASE("SceneLoader roundtrip saves and loads", "[scene][smoke]") {
    Scene scene;
    auto& reg = scene.Registry();
    auto entity = reg.create();

    reg.emplace<NameComponent>(entity, NameComponent{"Cube"});
    reg.emplace<Transform>(entity, Transform{1.0f, 2.0f, 3.0f, 0.1f, 0.2f, 0.3f, 1.0f, 1.0f, 1.0f});

    CameraComponent cam{};
    cam.primary = true;
    cam.fov = 60.0f;
    cam.nearPlane = 0.1f;
    cam.farPlane = 250.0f;
    reg.emplace<CameraComponent>(entity, cam);

    LightComponent light{};
    light.type = LightType::Directional;
    light.color[0] = 1.0f;
    light.color[1] = 0.9f;
    light.color[2] = 0.8f;
    light.intensity = 2.0f;
    reg.emplace<LightComponent>(entity, light);

    const fs::path scenePath = MakeTempScenePath("scene_roundtrip.scene");
    REQUIRE(SceneLoader::SaveScene(scene, scenePath.string()) == true);

    Scene loaded;
    REQUIRE(SceneLoader::LoadScene(loaded, scenePath.string()) == true);

    auto view = loaded.Registry().view<NameComponent, Transform>();
    bool found = false;
    for (auto e : view) {
        const auto& name = view.get<NameComponent>(e);
        const auto& t = view.get<Transform>(e);
        if (name.name == "Cube") {
            found = true;
            REQUIRE(t.x == Approx(1.0f));
            REQUIRE(t.y == Approx(2.0f));
            REQUIRE(t.z == Approx(3.0f));
            REQUIRE(t.rx == Approx(0.1f));
            REQUIRE(t.ry == Approx(0.2f));
            REQUIRE(t.rz == Approx(0.3f));
            REQUIRE(t.sx == Approx(1.0f));
            REQUIRE(t.sy == Approx(1.0f));
            REQUIRE(t.sz == Approx(1.0f));
        }
    }

    REQUIRE(found == true);
}

TEST_CASE("Scene runtime update rotates Cube", "[scene][runtime]") {
    Scene scene;
    auto& reg = scene.Registry();
    auto entity = reg.create();
    reg.emplace<NameComponent>(entity, NameComponent{"Cube"});
    reg.emplace<Transform>(entity, Transform{});

    scene.OnRuntimeStart();
    scene.OnUpdateRuntime(1.0);

    const auto& t = reg.get<Transform>(entity);
    REQUIRE(t.ry > 0.0f);
    REQUIRE(t.rx > 0.0f);

    scene.OnRuntimeStop();
}

TEST_CASE("Scene serialization is deterministic", "[scene][serialization]") {
    Scene scene;
    auto& reg = scene.Registry();

    auto parent = reg.create();
    reg.emplace<NameComponent>(parent, NameComponent{"Parent"});
    reg.emplace<Transform>(parent, Transform{0.0f, 1.0f, 2.0f, 0.0f, 0.25f, 0.0f, 1.0f, 1.0f, 1.0f});

    auto child = reg.create();
    reg.emplace<NameComponent>(child, NameComponent{"Child"});
    reg.emplace<Transform>(child, Transform{3.0f, 4.0f, 5.0f, 0.1f, 0.2f, 0.3f, 1.0f, 2.0f, 1.0f});
    reg.emplace<ParentComponent>(child, ParentComponent{parent});

    const fs::path scenePath = MakeTempScenePath("scene_deterministic.scene");
    REQUIRE(SceneLoader::SaveScene(scene, scenePath.string()) == true);
    const std::string first = ReadFileText(scenePath);

    REQUIRE(SceneLoader::SaveScene(scene, scenePath.string()) == true);
    const std::string second = ReadFileText(scenePath);
    REQUIRE(first == second);

    Scene loaded;
    REQUIRE(SceneLoader::LoadScene(loaded, scenePath.string()) == true);
    REQUIRE(SceneLoader::SaveScene(loaded, scenePath.string()) == true);
    const std::string third = ReadFileText(scenePath);
    REQUIRE(first == third);
}

TEST_CASE("Scene UI text layout survives roundtrip", "[scene][serialization][ui]") {
    Scene scene;
    auto& reg = scene.Registry();
    auto entity = reg.create();

    UIComponent ui;
    ui.type = UIType::Text;
    ui.text = "Hello UI";
    ui.width = 240.0f;
    ui.height = 80.0f;
    ui.useTextAlign = true;
    ui.textAlignH = UIAlignH::Right;
    ui.textAlignV = UIAlignV::Bottom;
    ui.textScale = 1.5f;
    ui.wrapText = true;
    ui.paddingX = 4.0f;
    ui.paddingY = 6.0f;
    ui.drawBorder = true;
    ui.borderThickness = 2.0f;
    ui.borderColor[0] = 0.1f;
    ui.borderColor[1] = 0.2f;
    ui.borderColor[2] = 0.3f;
    ui.borderColor[3] = 0.8f;
    reg.emplace<UIComponent>(entity, ui);

    const fs::path scenePath = MakeTempScenePath("scene_ui_roundtrip.scene");
    REQUIRE(SceneLoader::SaveScene(scene, scenePath.string()) == true);

    Scene loaded;
    REQUIRE(SceneLoader::LoadScene(loaded, scenePath.string()) == true);

    auto view = loaded.Registry().view<UIComponent>();
    REQUIRE(view.begin() != view.end());
    const auto& loadedUI = view.get<UIComponent>(*view.begin());

    REQUIRE(loadedUI.text == "Hello UI");
    REQUIRE(loadedUI.useTextAlign == true);
    REQUIRE(loadedUI.textAlignH == UIAlignH::Right);
    REQUIRE(loadedUI.textAlignV == UIAlignV::Bottom);
    REQUIRE(loadedUI.textScale == Approx(1.5f));
    REQUIRE(loadedUI.wrapText == true);
    REQUIRE(loadedUI.paddingX == Approx(4.0f));
    REQUIRE(loadedUI.paddingY == Approx(6.0f));
    REQUIRE(loadedUI.drawBorder == true);
    REQUIRE(loadedUI.borderThickness == Approx(2.0f));
    REQUIRE(loadedUI.borderColor[0] == Approx(0.1f));
    REQUIRE(loadedUI.borderColor[1] == Approx(0.2f));
    REQUIRE(loadedUI.borderColor[2] == Approx(0.3f));
    REQUIRE(loadedUI.borderColor[3] == Approx(0.8f));
}

TEST_CASE("Default scene loads and runs", "[scene][smoke]") {
    fs::path scenePath = FindDefaultScenePath();
    REQUIRE(!scenePath.empty());

    Scene scene;
    REQUIRE(SceneLoader::LoadScene(scene, scenePath.string()) == true);

    scene.OnRuntimeStart();
    scene.OnUpdateRuntime(0.016);
    scene.OnRuntimeStop();

    REQUIRE(scene.Registry().alive() > 0);
}
