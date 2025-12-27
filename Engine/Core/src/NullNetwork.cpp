#include "engine/INetwork.h"
#include "engine/SubsystemRegistry.h"
#include <iostream>

namespace Genesis::Engine {

class NullNetwork : public INetwork {
public:
    bool Init() override { std::cout << "NullNetwork: Init\n"; return true; }
    void Shutdown() override { std::cout << "NullNetwork: Shutdown\n"; }
    std::string Name() const override { return "null"; }

    bool Host(uint16_t /*port*/) override { return false; }
    bool Connect(const std::string& /*host*/, uint16_t /*port*/) override { return false; }
    void Poll(double /*dt*/) override {}
    void Update(double /*dt*/) override {}
    bool SendToAll(const Packet& /*data*/) override { return false; }
    bool Receive(Packet& /*out*/, int& /*peerId*/) override { return false; }
    bool IsHost() const override { return false; }
};

static bool register_null_network = []() {
    SubsystemRegistry::Instance().RegisterFactory("Network", "null", []() {
        return std::make_unique<NullNetwork>();
    });
    return true;
}();

void RegisterNullNetworkFactory() {
    SubsystemRegistry::Instance().RegisterFactory("Network", "null", []() {
        return std::make_unique<NullNetwork>();
    });
}

} // namespace Genesis::Engine
