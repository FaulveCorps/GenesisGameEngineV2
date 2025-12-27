# Genesis Game Engine - Build Setup

## Quick Start

### One-Command Setup (Recommended)

With **CMakePresets.json** and **vcpkg.json**, the project handles all dependency resolution automatically.

```bash
# Configure (debug build using Ninja)
cmake --preset default

# Build
cmake --build Build --config Debug

# Run tests
ctest --preset default
```

### Build Presets Available

**Debug builds:**
```bash
cmake --preset default                  # Configure debug
cmake --build Build --config Debug     # Build all (Debug)
cmake --build Build --target EngineCore --config Debug   # Build engine core only
cmake --build Build --target SampleGame --config Debug   # Build sample game only
cmake --build Build --target UnitTests --config Debug    # Build unit tests
```

**Release builds:**
```bash
cmake --preset release                 # Configure release
cmake --build Build --config Release
```

### Dependencies

The project uses `vcpkg.json` to declare dependencies, which are automatically resolved via CMake's vcpkg integration:

- **SDL3** - Window management and input
- **Assimp** - 3D model loading
- **EnTT** - Entity-component-system framework  
- **Catch2** - Unit testing framework

All packages are installed to `vcpkg/installed/x64-windows/` automatically on first configure.

### Build System

- **Generator**: Ninja (single-config) or Visual Studio 17 2022 (multi-config) - auto-selected by CMakePresets
- **Toolchain**: vcpkg CMake integration
- **Parallel Jobs**: Ninja uses all CPU cores by default; CMake passes concurrency to build tool

### Manual Configuration (if CMakePresets doesn't work)

If you prefer explicit control or need different settings:

```bash
# Clean previous build
rmdir /s /q Build

# Configure with explicit paths
cmake -S . -B Build ^
  -G "Ninja" ^
  -DCMAKE_BUILD_TYPE=Debug ^
  -DCMAKE_TOOLCHAIN_FILE=%CD%\vcpkg\scripts\buildsystems\vcpkg.cmake ^
  -DCMAKE_MAKE_PROGRAM=C:\path\to\ninja.exe ^
  -DVCPKG_TARGET_TRIPLET=x64-windows

# Build
cmake --build Build
```

### Troubleshooting

**CMake can't find Ninja:**
- Ensure Ninja is installed: `ninja --version`
- If not found, update CMAKE_MAKE_PROGRAM in CMakePresets.json to the correct path

**vcpkg dependency conflicts:**
- Delete `Build` folder and `vcpkg_installed` directory
- Re-run `cmake --preset default` to fresh-install all dependencies

**Build fails with compiler errors:**
- Check that Visual Studio C++ build tools are installed  
- Run `cmake --preset default --debug-output` for verbose configuration logs

### Continuous Integration

The setup is CI-ready. Example GitHub Actions workflow:

```yaml
- name: Configure
  run: cmake --preset default

- name: Build
  run: cmake --build Build --config Debug

- name: Test
  run: ctest --preset default
```

---

For more details, see the main [README.md](./README.md).
