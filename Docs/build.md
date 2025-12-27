# Build Instructions

Requirements
- CMake >= 3.16
- A suitable C++ toolchain (Visual Studio on Windows, GCC/Clang on Linux/macOS)
- SDL3 development libraries
- Assimp development libraries
- (Optional) glslangValidator for `shader_compiler` convenience

Out-of-source build

```bash
mkdir Build
cd Build
cmake ..
cmake --build . --config Debug
```

Using vcpkg (recommended for Windows/CI)

```bash
git clone https://github.com/Microsoft/vcpkg.git
./vcpkg/bootstrap-vcpkg.sh
cmake -S . -B Build -DCMAKE_TOOLCHAIN_FILE=path/to/vcpkg/scripts/buildsystems/vcpkg.cmake -DVCPKG_TARGET_TRIPLET=x64-windows
cmake --build Build --config Release
```

Packaging
- After building, you can create a ZIP package using CPack (configured by the project):

```bash
cmake --build Build --config Release --target package
```

This will create a `GenesisGameEngine-${PROJECT_VERSION}.zip` file in the build output.
