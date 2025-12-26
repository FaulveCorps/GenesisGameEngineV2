#include "engine/IAudio.h"
#include "engine/SubsystemRegistry.h"

#include <iostream>

namespace Genesis::Engine {

class NullAudio : public IAudio {
public:
    bool Init() override { std::cout << "NullAudio: Init\n"; return true; }
    void Shutdown() override { std::cout << "NullAudio: Shutdown\n"; }
    void Update(double /*dt*/) override {}
    std::string Name() const override { return "null"; }
    bool PlayOneShot(const std::string& /*assetPath*/, float /*volume*/ = 1.0f) override { return false; }
    void StopAll() override {}
};

static bool register_null_audio = []() {
    SubsystemRegistry::Instance().RegisterFactory("Audio", "null", []() {
        return std::make_unique<NullAudio>();
    });
    return true;
}();

} // namespace Genesis::Engine
