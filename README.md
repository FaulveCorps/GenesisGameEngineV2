# Genesis Game Engine

⚠️ **Repository re-rooted (2025-12-27)** — the repository was re-rooted so that the previous `GameEngine/` subtree is now the repository root. See `REROOT.md` for backups & recovery instructions and next steps for collaborators (re-clone or reset local branches).

Genesis is a modular, cross-platform C++ game engine prototype. This repository contains the engine core, renderers, tools, and a sample game used for verification.

## Features
- **Rendering**: Modular backend supporting OpenGL (PBR), Vulkan, DirectX, and Software rendering.
  - **PBR Pipeline**: Physically Based Rendering with glTF 2.0 support (Albedo, Normal, Metallic/Roughness).
  - **Runtime Switching**: Hot-swap renderers at runtime (F2 key).
- **Architecture**: ECS-based (Entity Component System) using `entt`.
- **Scripting**: 
  - **Lua**: Standard game logic scripting.
  - **WASM**: Secure, sandboxed modding support via `wasm3`.
- **Physics**: Modular backend supporting Bullet and Box2D.
- **Platform**: Windows (primary), Linux/macOS (via CMake).

## Status
See [Docs/status_report_20260103.md](Docs/status_report_20260103.md) for the latest detailed status and competitive analysis.

## Getting started
Requirements:
- CMake >= 3.16
- Visual Studio 2022 (recommended)
- `vcpkg` (integrated automatically)

Build (Windows):
```powershell
./build.ps1
```

This script uses CMake Presets to configure and build the project with all dependencies (SDL3, Assimp, etc.) managed by vcpkg.

## Tools
- `asset_packer`: Pack/unpack assets.
- `shader_compiler`: Compile GLSL to SPIR-V.
- `Scripts/generate_gltf.py`: Generate test PBR assets.

## CI/CD
- **Build**: GitHub Actions (Windows/Linux/macOS).
- **Security**: Snyk SAST integration for vulnerability scanning.


Packaging:
- Use CPack to create a ZIP package of the built artifacts. Example:

```bash
cmake -S . -B Build -DCMAKE_BUILD_TYPE=Release
cmake --build Build --config Release --target package
```

This will create `GenesisGameEngine-${PROJECT_VERSION}.zip` in the Build output. You can also install the project contents to a local prefix with `cmake --install Build --config Release --prefix <dir>`.
