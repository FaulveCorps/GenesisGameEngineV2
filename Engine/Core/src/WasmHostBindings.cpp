#ifdef HAVE_WASM3
#include "engine/WasmHostBindings.h"
#include <iostream>
#include "wasm3.h"

namespace Genesis::Engine {

// Stub used when unregistering a host function — keeps the import present but traps if called.
m3ApiRawFunction(host_unregistered_stub) {
    // Trap with a descriptive string to help debugging if an unregistered import is invoked
    m3ApiTrap("host function unregistered");
}

HostBindings::Token HostBindings::RegisterRaw(const char* ns, const char* name, const char* sig, M3RawCall cb) {
    if (!module_) return {};
    M3Result r = m3_LinkRawFunction(module_, ns, name, sig, cb);
    if (r) {
        std::cerr << "HostBindings: m3_LinkRawFunction failed for '" << ns << "'.'" << name << "' sig='" << sig << "': " << r << std::endl;
        return {};
    }
    regs_.emplace_back(std::string(ns), std::string(name), std::string(sig), cb);
    return Token(this, std::string(ns), std::string(name), std::string(sig));
}

void HostBindings::Unregister(const std::string& ns, const std::string& name, const std::string& sig) noexcept {
    if (!module_) return;
    // Replace the function with a stub to avoid leaving a dangling pointer into host code.
    M3Result r = m3_LinkRawFunction(module_, ns.c_str(), name.c_str(), sig.c_str(), (M3RawCall)host_unregistered_stub);
    if (r) {
        // best-effort logging; we must not throw from noexcept destructor
        std::cerr << "HostBindings: failed to unregister host '" << ns << "'.'" << name << "': " << r << std::endl;
    }
    // Remove from our registry for bookkeeping
    regs_.erase(std::remove_if(regs_.begin(), regs_.end(), [&](auto &t) {
        return std::get<0>(t) == ns && std::get<1>(t) == name && std::get<2>(t) == sig;
    }), regs_.end());
}

HostBindings::Token::~Token() noexcept {
    if (owner_) {
        owner_->Unregister(ns_, name_, sig_);
        owner_ = nullptr;
    }
}

} // namespace Genesis::Engine
#endif // HAVE_WASM3
