#include "engine/ISave.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>

namespace Genesis::Engine {

class NullSave : public ISave {
public:
    bool Init() override { std::cout << "NullSave: Init\n"; return true; }
    void Update(double /*dt*/) override { /* no-op */ }
    void Shutdown() override { std::cout << "NullSave: Shutdown\n"; }
    std::string Name() const override { return "null"; }

    bool Save(const std::string& /*slot*/, const std::string& /*data*/) override { return false; }
    bool Load(const std::string& /*slot*/, std::string& /*out*/) override { return false; }
    bool Delete(const std::string& /*slot*/) override { return false; }
    std::vector<std::string> ListSlots() const override { return {}; }
};

static bool register_null_save = []() {
    SubsystemRegistry::Instance().RegisterFactory("Save", "null", []() {
        return std::make_unique<NullSave>();
    });
    return true;
}();

void RegisterNullSaveFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Save", "null", []() {
        return std::make_unique<NullSave>();
    });
}

} // namespace Genesis::Engine
