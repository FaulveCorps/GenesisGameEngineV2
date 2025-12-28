# ADR 2025-12-28: Typesafe Wasm Host Bindings & Runtime Hardening

**Status:** Proposed

**Date:** 2025-12-28

## Context / Problem

- Host bindings are currently registered with raw `m3_LinkRawFunction` calls and macros. This is error-prone and scatters manual pointer handling across the codebase.
- The runtime lacks consistent resource limits (execution time, memory caps), robust input validation, and clear lifecycle semantics for host registration tokens. This increases crash and DoS risk for untrusted or buggy modules.

## Decision

Introduce a small, typesafe `HostBindings` helper (namespace: `Genesis::Engine::Wasm`) that:

- Provides RAII registration tokens that unregister automatically when dropped.
- Exposes typesafe helpers (common signatures) and memory helpers (read/alloc/free) on top of raw registration.
- Keeps a raw `register_raw` compat path to `m3_LinkRawFunction` for low-level needs.
- Harden `WasmRuntime` with execution timeouts, memory caps, validation checks and a watchdog facility by default (configurable).

Rationale: minimize bugs, provide clear migration path from raw APIs, and improve security and maintainability.

## Proposed API (summary)

Create `Engine/Core/include/Engine/WasmHostBindings.h` (or update existing file) with a concise API, e.g.:

```cpp
namespace Genesis::Engine::Wasm {

class HostBindings {
public:
    explicit HostBindings(IM3Runtime* runtime) noexcept;
    ~HostBindings();

    struct Registration { /* move-only RAII token, has unregister() */ };

    Registration register_raw(const char* module, const char* name, const char* sig, void* raw_fn);

    template<typename Fn>
    Registration register_fn(const char* module, const char* name, Fn&& callable);

    Registration register_i32_fn(const char* module, const char* name, std::function<int32_t(int32_t)>);

    Registration register_void_string_fn(const char* module, const char* name, std::function<void(const std::string&)> fn);

    std::string read_string(uint32_t ptr, uint32_t len) const;
    uint32_t alloc_in_module(const std::string& s);
    void free_in_module(uint32_t ptr) const;

    void set_max_string_length(size_t bytes);
    void set_execution_timeout_ms(uint32_t ms);
};

}
```

Short usage:

```cpp
Wasm::HostBindings host(runtime);
auto t = host.register_void_string_fn("env","host_log",[](const std::string&s){ LOG_INFO("%s",s.c_str()); });
```

## Migration

- Add ADR + the header stub.
- Implement HostBindings & tests.
- Migrate internal code (SamplePlugin, SampleGame) incrementally, PR-per-module.
- Deprecate raw usages with warnings and remove after a 6-month deprecation period.

Codemod example: replace `m3_LinkRawFunction(module, "env", "print", "i(i)", &host_print)` with `host.register_fn<void(int32_t)>("env","print", host_print);`

## Testing

- Unit tests: `Tests/test_wasm_host_bindings.cpp` (read/alloc, RAII semantics, error cases).
- Integration tests with small `.wat` modules in `Tests/data/wasm/host_bindings/`.
- CI: add CMake test target and CI job to run wasm tests (ensure `wat2wasm` or precompiled `.wasm` present).

Acceptance: tests pass on CI; no regressions in other test targets.

## Rollout & PR breakdown

- `feat/wasm-host-bindings/adr` — ADR + header stub (small PR)
- `feat/wasm-host-bindings/api` — API implementation + unit tests
- `feat/wasm-host-bindings/impl` — complete impl + integration tests
- `feat/wasm-runtime-hardening` — timeouts, memory caps, watchdog
- `chore/wasm-migrate-*` — migrate modules one-by-one

Each PR: small, test-covered, includes CHANGELOG note and PR checklist.

## Security & Resource Safety

- Validate all pointer+length pairs against module memory.
- Limit default max string length (e.g., 1MB) and per-call execution timeout (e.g., 1000ms), configurable.
- Add watchdog to abort runaway executions.
- Request a security review for runtime hardening PR(s).

## Risks & Alternatives

- Full C++ wrapper (complete runtime rewrite): more safety but large scope — postponed.
- Macro-only approach: low effort but no typesafety — rejected.
- Selected: Small helper API + templates for the most used patterns.

## Files to add / modify

- `Docs/adr/2025-12-28-wasm-host-bindings.md` (this file)
- `Engine/Core/include/ENGINE/WasmHostBindings.h` (API)
- `Engine/Core/src/WasmHostBindings.cpp` (impl)
- `Tests/test_wasm_host_bindings.cpp`
- `Tests/data/wasm/host_bindings/sample.wat`
- CI config modifications to run wasm tests

**Initial commit message:** `docs(adr): propose typesafe Wasm HostBindings and runtime hardening`

---

If approved, next step: implement header stub + small ADR PR and then the API implementation with unit tests. I recommend we schedule a short security review for the hardening changes before merging.