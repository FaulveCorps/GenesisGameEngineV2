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
    // debug: print module pointer and the binding we're attempting
    IM3Runtime modRuntime = m3_GetModuleRuntime(module_);
    const char* modName = m3_GetModuleName(module_);
    std::cerr << "HostBindings: RegisterRaw module=" << module_ << " name='" << (modName ? modName : "(null)") << "' runtime=" << modRuntime << " ns='" << ns << "' name='" << name << "' sig='" << sig << "'" << std::endl;
    M3Result r = m3_LinkRawFunction(module_, ns, name, sig, cb);
    if (r) {
        std::cerr << "HostBindings: m3_LinkRawFunction failed for '" << ns << "'.'" << name << "' sig='" << sig << "': " << r << std::endl;
        return {};
    }
    // Return a token that stores the module pointer so it can unregister itself later.
    return Token(module_, std::string(ns), std::string(name), std::string(sig));
}


HostBindings::Token::~Token() noexcept {
    if (module_) {
        // Replace the function with a stub to avoid leaving a dangling pointer into host code.
        M3Result r = m3_LinkRawFunction(module_, ns_.c_str(), name_.c_str(), sig_.c_str(), (M3RawCall)host_unregistered_stub);
        if (r) {
            // If the signature doesn't match (can happen if the module import signature differs),
            // attempt a best-effort fallback by linking with a null signature which skips validation.
            // This is a safe, best-effort cleanup and should not throw.
            const char* msg = r ? r : "(unknown)";
            if (msg && std::string(msg).find("function signature mismatch") != std::string::npos) {
                M3Result r2 = m3_LinkRawFunction(module_, ns_.c_str(), name_.c_str(), nullptr, (M3RawCall)host_unregistered_stub);
                if (!r2) {
                    // success on fallback; be quiet (no error to report)
                } else {
                    std::cerr << "HostBindings: failed to unregister host '" << ns_ << "'.'" << name_ << "' (fallback also failed): " << r2 << std::endl;
                }
            } else {
                // best-effort logging; we must not throw from noexcept destructor
                std::cerr << "HostBindings: failed to unregister host '" << ns_ << "'.'" << name_ << "': " << r << std::endl;
            }
        }
        module_ = nullptr;
    }
}

} // namespace Genesis::Engine
#endif // HAVE_WASM3
