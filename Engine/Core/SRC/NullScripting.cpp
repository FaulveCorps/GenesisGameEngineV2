#include "engine/IScripting.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>

namespace Genesis::Engine {

class NullScripting : public IScripting {
public:
    bool Init() override { std::cout << "NullScripting: Init\n"; return true; }
    void Update(double /*dt*/) override { /* no-op */ }
    void Shutdown() override { std::cout << "NullScripting: Shutdown\n"; }
    std::string Name() const override { return "null"; }

    bool ExecuteString(const std::string& /*code*/) override { return false; }
    bool ExecuteFile(const std::string& /*path*/) override { return false; }
};

static bool register_null_scripting = []() {
    SubsystemRegistry::Instance().RegisterFactory("Scripting", "null", []() {
        return std::make_unique<NullScripting>();
    });
    return true;
}();

void RegisterNullScriptingFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Scripting", "null", []() {
        return std::make_unique<NullScripting>();
    });
}

} // namespace Genesis::Engine
