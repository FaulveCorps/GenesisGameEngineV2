#pragma once

#ifdef HAVE_WASM3
#include "wasm3.h"
#include <string>
#include <vector>
#include <tuple>

namespace Genesis::Engine {

// Small helper to register host imports for a module and get an RAII token
// that will auto-unregister the binding when destroyed.
class HostBindings {
public:
    explicit HostBindings(IM3Module module) : module_(module) {}
    HostBindings(const HostBindings&) = delete;
    HostBindings& operator=(const HostBindings&) = delete;
    HostBindings(HostBindings&&) = delete;
    HostBindings& operator=(HostBindings&&) = delete;

    struct Token {
        Token() = default;
        Token(Token&& other) noexcept : module_(other.module_), ns_(std::move(other.ns_)), name_(std::move(other.name_)), sig_(std::move(other.sig_)) { other.module_ = nullptr; }
        Token& operator=(Token&&) = delete;
        ~Token() noexcept;
        bool valid() const noexcept { return module_ != nullptr; }

    private:
        friend class HostBindings;
        Token(IM3Module module, std::string ns, std::string name, std::string sig) noexcept : module_(module), ns_(std::move(ns)), name_(std::move(name)), sig_(std::move(sig)) {}

        IM3Module module_ = nullptr;
        std::string ns_;
        std::string name_;
        std::string sig_;
    };

    // Register a raw host function (namespace, name, signature, callback). Returns a Token
    // that will unregister on destruction. The callback must conform to wasm3's M3RawCall.
    Token RegisterRaw(const char* ns, const char* name, const char* sig, M3RawCall cb);

private:
    IM3Module module_ = nullptr;
};

} // namespace Genesis::Engine
#endif // HAVE_WASM3
