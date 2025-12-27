# ADR 0003 — Robust WasmRuntime & Host-Binding API

Date: 2025-12-27
Status: Proposed

## Context

The engine currently embeds wasm3 via our vcpkg port and supports loading basic WASM modules. We need a robust runtime with safe lifecycle management, a lightweight, typed host-binding API, and enforceable resource limits to avoid guest-induced outages.

## Decision

We adopt a new `WasmRuntime` abstraction and a minimal, explicit host-binding API with the following decisions:

1. **Host-binding API**
   - Provide `RegisterHostFunction(module_ns, func_name, signature, callback)` returning an RAII `HostRegistration` token object that unregisters on destruction.
   - Support typed signatures for primitive types and common buffers (`i32`, `i64`, `f32`, `f64`, `ptr/len` for arrays/strings).
   - Fail fast on signature mismatch with clear error codes/logs.

2. **RAII / Lifecycle**
   - `WasmRuntime` and `WasmModule` are RAII-managed (destructor cleans up runtime and module instances and host registrations).
   - Use `unique_ptr` for single ownership of internal runtime resources and `shared_ptr` for modules if cross-component sharing is required.
   - All destructors are `noexcept` and guarantee cleanup even if the module is active (in-flight calls allowed to finish or be cancelled by a watchdog).

3. **Resource Limits & Policy**
   - Introduce a `ResourceLimits` struct with defaults:
     - `memory_limit_bytes = 64 * 1024 * 1024` (64MB)
     - `execution_time_ms = 2000` (2s)
     - `instruction_budget = optional, implemented if host engine supports it`
   - By default, limits are enforced and configurable per runtime/module. Exceeding limits causes a deterministic trap and a visible error code instead of crash.

## Consequences

- Safer default behaviour: untrusted modules cannot silently hog memory/CPU.
- Host bindings are easier to use and less error prone.
- Backward compatibility maintained by offering the refactor behind a feature flag until validated.

## Alternatives considered

- Auto-binding generation from a schema/WASI: **Rejected** for scope and complexity.
- Keep current C-style binding APIs: **Rejected** due to high risk of leaks and unclear lifetimes.

---

*Notes*: Commit this ADR before large refactor PRs and reference it in PR descriptions.
