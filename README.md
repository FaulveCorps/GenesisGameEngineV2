# Genesis Game Engine

Genesis is a modular, cross-platform C++ game engine prototype. This repository contains the engine core, renderers, tools, and a sample game used for verification.

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

Next step: implement Task 3 — add Core window + input (SDL3) minimal app.

Tools:
- `asset_packer` (Tools/asset_packer) — pack/unpack asset directories into a simple .ggpak archive. Usage: `asset_packer pack <input_dir> <out_file>` or `asset_packer unpack <archive> <out_dir>`.
- `shader_compiler` (Tools/shader_compiler) — compiles GLSL to SPIR-V using `glslangValidator` if available, otherwise copies the source as a fallback. Usage: `shader_compiler <input_shader> <output_spv>`.
