#pragma once

#ifdef HAVE_WASM3
#include "wasm3.h"
#include <string>
#include <vector>
#include <tuple>
#include <functional>
#include <cstdint>
#include <new>
#include <atomic>

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
        Token(Token&& other) noexcept : module_(other.module_), host_(other.host_), ns_(std::move(other.ns_)), name_(std::move(other.name_)), sig_(std::move(other.sig_)) { other.module_ = nullptr; other.host_ = nullptr; }
        Token& operator=(Token&& other) noexcept {
            if (this == &other) return *this;
            // Ensure existing registration (if any) is cleaned up
            this->~Token();
            // Move-construct into this storage
            new (this) Token(std::move(other));
            return *this;
        }
        ~Token() noexcept;
        bool valid() const noexcept { return module_ != nullptr; }

    private:
        friend class HostBindings;
        Token(IM3Module module, HostBindings* host, std::string ns, std::string name, std::string sig) noexcept : module_(module), host_(host), ns_(std::move(ns)), name_(std::move(name)), sig_(std::move(sig)) {}

        IM3Module module_ = nullptr;
        HostBindings* host_ = nullptr;
        std::string ns_;
        std::string name_;
        std::string sig_;
        // If true, module is managed by WasmRuntime; affects safe unregistration behavior
        bool module_managed_ = false;
    };

    // Register a raw host function (namespace, name, signature, callback). Returns a Token
    // that will unregister on destruction. The callback must conform to wasm3's M3RawCall.
    // Optional last parameter: pointer to the module's timed_out flag (if caller already has it)
    Token RegisterRaw(const char* ns, const char* name, const char* sig, M3RawCall cb, std::atomic<bool>* module_timed_out_ptr = nullptr);

    // Register a host function that takes/returns i32 (i32 -> i32). Useful for simple numeric callbacks.
    // Returns an RAII Token that will unregister the binding when destroyed.
    Token RegisterI32I32(const char* ns, const char* name, std::function<int32_t(int32_t)> cb);

    // Register a host function that takes (ptr: i32, len: i32) as a string parameter.
    // The helper will read the bytes from the module memory, bounds-check using
    // set_max_string_length(), and invoke the supplied std::function with an std::string.
    Token RegisterVoidString(const char* ns, const char* name, std::function<void(const std::string&)> cb);

    // Read a string from the module's linear memory (ptr, len). Performs bounds checks and
    // respects the configured maximum string length.
    // Returns empty string on any error (invalid module, OOB, or len > max).
    std::string read_string(uint32_t ptr, uint32_t len) const;

    // Best-effort: attempt to call an exported `alloc` (or `__alloc`) function to allocate
    // `s.size()` bytes in the module memory and return the pointer. If no suitable alloc
    // export is found or the call fails, returns 0.
    uint32_t alloc_in_module(const std::string& s);

    // Stubs / configuration helpers
    void set_max_string_length(size_t bytes) { maxStringLength_ = bytes; }
    void set_execution_timeout_ms(uint32_t ms) { executionTimeoutMs_ = ms; /* TODO: implement enforcement */ }

    // Cleanup helper that will remove any callbacks associated with a given module.
    // Useful to ensure any holders are destroyed only after the module/runtime are gone.
    static void CleanupModuleCallbacks(IM3Module module);

private:
    IM3Module module_ = nullptr;
    size_t maxStringLength_ = 1024 * 1024; // 1 MB default
    uint32_t executionTimeoutMs_ = 1000;   // 1s default (not enforced yet)
};

} // namespace Genesis::Engine
#endif // HAVE_WASM3
