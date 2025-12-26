# Networking Subsystem

Overview
--------
The Networking subsystem provides a pluggable interface for networked features (multiplayer, remote control, telemetry).
The interface is `INetwork` (see `Engine/Core/include/ENGINE/INetwork.h`).

Current backends
----------------
- `null` — no-op backend (always available)
- Future: `enet` backend will be added as an optional dependency (ENet via vcpkg)

Key API
-------
- `Host(port)` — attempt to host a server on the given port.
- `Connect(host, port)` — attempt to connect to a remote host.
- `Poll(dt)` — poll network events; should be called regularly.
- `SendToAll(data)` / `Receive()` — basic send/receive packet primitives.

Usage
-----
1. Initialize engine: `Genesis::Engine::Init()` (registers networking factories)
2. Create or switch to a backend:

```cpp
if (!Genesis::Engine::CreateNetworkSubsystem("enet")) {
    // fallback to null
    Genesis::Engine::CreateNetworkSubsystem("null");
}
```

3. Use `Host`/`Connect`, call `Poll` periodically, and use `SendToAll`/`Receive` to exchange data.

Notes
-----
- The `null` backend is intentionally minimal and returns safe defaults.
- When an ENet backend is added it will be guarded by a `HAVE_ENET` compile-time flag and integrated into CMake similar to other optional dependencies.

Extending
---------
To add another backend implement `INetwork`, register a factory:

```cpp
SubsystemRegistry::Instance().RegisterFactory("Network", "yourname", [](){
    return std::make_unique<YourNetwork>();
});
```

Then select it at runtime via `CreateNetworkSubsystem("yourname")`.
