#include "catch_amalgamated.hpp"
#include "engine/Engine.h"
#include "engine/ISave.h"

#include <filesystem>
#include <string>
#include <optional>
#include <chrono>
#include <thread>

#ifdef _WIN32
#include <stdlib.h>
#else
#include <stdlib.h>
#endif

using namespace Genesis::Engine;

static std::string UniqueTempDir() {
    auto now = std::chrono::steady_clock::now().time_since_epoch().count();
    auto tmp = std::filesystem::temp_directory_path();
    tmp /= ("gen_save_test_" + std::to_string(now));
    return tmp.string();
}

class ScopedEnvVar {
public:
    ScopedEnvVar(const std::string& name, const std::string& value)
        : m_name(name) {
        const char* old = std::getenv(name.c_str());
        if (old) m_old = old;
#ifdef _WIN32
        _putenv_s(name.c_str(), value.c_str());
#else
        setenv(name.c_str(), value.c_str(), 1);
#endif
    }
    ~ScopedEnvVar() {
        if (m_old.has_value()) {
#ifdef _WIN32
            _putenv_s(m_name.c_str(), m_old->c_str());
#else
            setenv(m_name.c_str(), m_old->c_str(), 1);
#endif
        } else {
#ifdef _WIN32
            _putenv_s(m_name.c_str(), "");
#else
            unsetenv(m_name.c_str());
#endif
        }
    }
private:
    std::string m_name;
    std::optional<std::string> m_old;
};

TEST_CASE("SaveSubsystem: Null backend behavior", "[save]") {
    REQUIRE(Genesis::Engine::Init() == true);

    REQUIRE(Genesis::Engine::CreateSaveSubsystem("null") == true);
    auto sv = Genesis::Engine::GetSaveSubsystem();
    REQUIRE(sv != nullptr);
    REQUIRE(sv->Name() == "null");

    REQUIRE(sv->Save("slot1", "data") == false);

    std::string out;
    REQUIRE(sv->Load("slot1", out) == false);

    REQUIRE(sv->Delete("slot1") == false);

    auto slots = sv->ListSlots();
    REQUIRE(slots.empty());
}

TEST_CASE("SaveSubsystem: File backend basic roundtrip", "[save]") {
    // Use a unique temp dir and set env so FileSave uses it
    auto tmp = UniqueTempDir();
    ScopedEnvVar env("GENESIS_SAVE_DIR", tmp);

    // Create the dir path before SDK init to be safe
    std::filesystem::create_directories(tmp);

    REQUIRE(Genesis::Engine::Init() == true);
    REQUIRE(Genesis::Engine::CreateSaveSubsystem("file") == true);
    auto sv = Genesis::Engine::GetSaveSubsystem();
    REQUIRE(sv != nullptr);
    REQUIRE(sv->Name() == "file");

    // Save data
    REQUIRE(sv->Save("testslot", "hello_world") == true);

    // Load back
    std::string out;
    REQUIRE(sv->Load("testslot", out) == true);
    REQUIRE(out == "hello_world");

    // Slots list
    auto slots = sv->ListSlots();
    REQUIRE(slots.size() == 1);
    REQUIRE(slots[0] == "testslot");

    // Delete slot
    REQUIRE(sv->Delete("testslot") == true);
    REQUIRE(sv->Load("testslot", out) == false);

    // Cleanup dir
    std::filesystem::remove_all(tmp);
}
