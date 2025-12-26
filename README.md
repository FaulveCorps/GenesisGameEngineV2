# Genesis Game Engine

Genesis is a modular, cross-platform C++ game engine prototype. This repository contains the engine core, renderers, tools, and a sample game used for verification.

Project governance artifacts (agent manifests, ADRs, and policies) are stored in the `Agenda/` directory to support Agenda+ workflows.

## Getting started (initial skeleton)
Requirements:
- CMake >= 3.16
- Visual Studio (recommended) or a suitable C++ toolchain
- SDL3 development libraries (headers and import libs) to build and run the desktop windowing sample
- Assimp development libraries for model import support
- Dear ImGui (fetched automatically by CMake) for debug overlays

On Windows you can install SDL3 and Assimp or provide the development packages to CMake via your preferred method (vcpkg / MSYS2 / system install).

Build (out-of-source):

```bash
mkdir build
cd build
cmake ..
cmake --build . --config Debug
```

This initial scaffold builds a small `EngineCore` static library and a `SampleGame` executable used for smoke tests. Later steps will integrate SDL3, OpenGL, Assimp, EnTT, and ImGui.

Runtime renderer switching: The engine supports runtime switching between renderers (e.g., OpenGL, Vulkan, WGPU, DirectX, Software). Shaders and textures are now tracked by registries and will be re-created on renderer switches where supported.

Artifacts: runtime screenshots and automatic test outputs are written to an `artifacts/` directory; this directory is ignored by default (see `.gitignore`).

Next step: implement Task 3 — add Core window + input (SDL3) minimal app.

Tools:
- `asset_packer` (Tools/asset_packer) — pack/unpack asset directories into a simple .ggpak archive. Usage: `asset_packer pack <input_dir> <out_file>` or `asset_packer unpack <archive> <out_dir>`.
- `shader_compiler` (Tools/shader_compiler) — compiles GLSL to SPIR-V using `glslangValidator` if available, otherwise copies the source as a fallback. Usage: `shader_compiler <input_shader> <output_spv>`.

CI:
- A GitHub Actions workflow is included at `.github/workflows/ci.yml` which builds the project on **Ubuntu**, **macOS**, and **Windows** using `vcpkg` to fetch dependencies (SDL3, Assimp). The workflow runs on push and PR to `main`/`master` and performs a full CMake configure + build step.

Packaging:
- Use CPack to create a ZIP package of the built artifacts. Example:

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target package
```

This will create `GenesisGameEngine-${PROJECT_VERSION}.zip` in the build output. You can also install the project contents to a local prefix with `cmake --install build --config Release --prefix <dir>`.
