# PR: wasm/resource-limits watchdog

Summary
-------
This PR adds basic resource enforcement and a time-based watchdog for the Wasm runtime:

- Add `ResourceLimits` struct with defaults: 64MB memory, 2000ms execution timeout.
- Implement `WasmRuntime::CallExportedWithTimeout(module, func, args, timeoutMs)` which runs the call asynchronously and enforces a timeout; modules that exceed the limit are marked as `timed_out` and subsequent calls are rejected.
- Update `WasmRuntime::CallExported` to use the default execution_time_ms resource limit.
- Schedule deferred cleanup of timed-out modules to avoid freeing runtime resources while guest code is executing.
- Add unit tests (`Tests/test_wasm_limits.cpp`) that validate timeout behavior using an embedded wasm module and a host sleep import.
- Add TODOs and ticket note in `Docs/adr/0003-wasm-runtime.md` to track per-module memory limit enforcement.

Files changed
-------------
- Engine/Core/include/ENGINE/Wasm/ResourceLimits.h  (new)
- Engine/Core/include/ENGINE/WasmRuntime.h        (declaration updates)
- Engine/Core/src/WasmRuntime.cpp                 (watchdog implementation, TODOs)
- Tests/test_wasm_limits.cpp                      (new test)
- Tests/CMakeLists.txt                            (append test)
- Docs/adr/0003-wasm-runtime.md                   (TODO added)

How to test
-----------
- Build and run UnitTests (requires wasm3 available in vcpkg):
  - cmake --build build -t UnitTests --config Debug
  - ctest -C Debug -R UnitTests
- The new test `WasmRuntime: execution timeout and module quarantine` should pass when `wasm3` is present.
- If `wasm3` is not available locally, tests are skipped.

Notes / Future work
-------------------
- Memory limits (per-module max pages) currently NOT enforced at load time. See ADR TODO and ticket GS-XXXX.
- Consider integration with wasm3 or the host to clamp module memory growth and to implement instruction budgets.

Closes / Related Issues
-----------------------
- (Create/attach issue for per-module memory enforcement: GS-XXXX)

Reviewer notes
--------------
- The watchdog marks modules as `timed_out` and schedules cleanup later to avoid freeing runtime while a guest thread is still executing.
- Logging includes module and function names and timeout duration for better debuggability.
