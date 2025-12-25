# WGPU / Dawn backend

This project supports an optional native WebGPU backend via Dawn (enabled when `HAVE_WGPU` is defined by CMake).

Quick setup (Windows, vcpkg):

1. Bootstrap vcpkg if needed:
   - `.ootstrap-vcpkg.bat` (Windows)

2. Install Dawn via vcpkg:
   - `.\vcpkg.exe install dawn:x64-windows`

3. Configure CMake to point to the vcpkg installed tree (example):
   - `cmake -S . -B build -G "Ninja" -A x64 -DCMAKE_BUILD_TYPE=Debug -DVCPKG_INSTALLED="<path-to-repo>\vcpkg\installed\x64-windows"`

4. Build & run the SampleGame with the `wgpu` backend:
   - `cmake --build build --config Debug --target SampleGame`
   - `build\GameProjects\SampleGame\Debug\SampleGame.exe --gfx-order wgpu`

Notes:
- The GitHub Actions workflow `windows-d3d12.yml` contains a `wgpu` matrix entry that will attempt to install `dawn:x64-windows` and run the smoke tests (including unit tests and the SampleGame smoke run).
- The unit tests now include a WGPU smoke test that renders an offscreen triangle and read-backs a small image to verify the triangle was drawn (center pixel asserted to be red).
- When Dawn is detected, CMake will copy the necessary runtime DLLs (`webgpu_dawn.dll`, `abseil_dll.dll`) into test/SampleGame output directories; on Windows the build also attempts to copy MSVC CRT DLLs from a local Visual Studio redist folder if present to make test output portable.
- If `dawn` is not installed or not available on the platform, the build will not enable `HAVE_WGPU` and the WGPU renderer will be skipped at runtime.
- On Windows the engine derives an `HWND` from the SDL window and creates a `WGPUSurface` using the `WGPUSurfaceDescriptorFromWindowsHWND` chain.
- The renderer installs an uncaptured device error callback and will attempt a graceful re-initialization if the device is lost; this attempts to recover without requiring a full process restart.

Troubleshooting:
- If CMake does not detect Dawn, verify the presence of `vcpkg/installed/x64-windows/include/dawn/dawn_proc.h` and `vcpkg/installed/x64-windows/lib/webgpu_dawn.lib`.
- If the unit tests exit immediately with code 0xC0000135 (missing module), ensure the Visual C++ Redistributable (MSVC CRT) is installed on the system or that the redist DLLs are available in the build output (CMake will try to copy them if a local Visual Studio redist is detected).
- Building `dawn` from source via vcpkg can be time-consuming; consider using a cached vcpkg installed tree in CI.
