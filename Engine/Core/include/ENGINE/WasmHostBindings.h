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
        Token(Token&& other) noexcept : owner_(other.owner_), ns_(std::move(other.ns_)), name_(std::move(other.name_)), sig_(std::move(other.sig_)) { other.owner_ = nullptr; }
        Token& operator=(Token&&) = delete;
        ~Token() noexcept;
        bool valid() const noexcept { return owner_ != nullptr; }

    private:
        friend class HostBindings;
        Token(HostBindings* owner, std::string ns, std::string name, std::string sig) noexcept : owner_(owner), ns_(std::move(ns)), name_(std::move(name)), sig_(std::move(sig)) {}

        HostBindings* owner_ = nullptr;
        std::string ns_;
        std::string name_;
        std::string sig_;
    };

    // Register a raw host function (namespace, name, signature, callback). Returns a Token
    // that will unregister on destruction. The callback must conform to wasm3's M3RawCall.
    Token RegisterRaw(const char* ns, const char* name, const char* sig, M3RawCall cb);

private:
    void Unregister(const std::string& ns, const std::string& name, const std::string& sig) noexcept;

    IM3Module module_ = nullptr;
    // track registrations so we can find and remove them
    std::vector<std::tuple<std::string, std::string, std::string, M3RawCall>> regs_;
};

} // namespace Genesis::Engine
#endif // HAVE_WASM3
