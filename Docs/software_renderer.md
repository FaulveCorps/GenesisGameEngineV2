# Software (CPU) Renderer

**Purpose:** A small, deterministic CPU rasterizer useful for headless CI, smoke tests, and environments without a GPU.

**Key points**
- Implements `Genesis::Engine::IGraphicsAPI` as `SoftwareRenderer`.
- Exposes `ReadbackOffscreen(width, height, out)` to render a simple test pattern into a BGRA8 buffer suitable for deterministic assertions in unit tests.
- No external GPU dependencies; useful to validate rendering logic and CI runs on GPUs not present on runners.

**How to use**
- Force the software renderer via `GraphicsFactory::CreateRenderer` priority order:

```cpp
auto renderer = Genesis::Engine::GraphicsFactory::CreateRenderer(win, ctx, {"software"}, false);
```

- After `Init`, you can call `BeginFrame()` / `EndFrame()` and then, for offscreen validation, call:

```cpp
std::vector<uint8_t> pixels;
if (auto* sr = dynamic_cast<Genesis::Engine::SoftwareRenderer*>(renderer.get())) {
    sr->ReadbackOffscreen(64, 64, pixels); // BGRA8
}
```

**Notes for CI**
- The project now includes `Tests/test_software_smoke.cpp` which uses the SoftwareRenderer to perform a deterministic offscreen render and verify pixel values. This test is part of the `UnitTests` suite and will run on CI without requiring GPU drivers.

**Running a visual proof (SampleGame)**
- Build and run the SampleGame with the software renderer forced:

```
SampleGame.exe --gfx-order software
```

- When the software renderer is selected, SampleGame creates a secondary window called **Software Output** and presents the CPU-rendered image in real time (BGRA pixels). If the platform cannot create the secondary window (headless CI, missing windowing support), SampleGame will save the first captured frame to `software_render.bmp` in the current working directory as a fallback.

**Future**
- The SoftwareRenderer is intentionally minimal and intended for testing; for production or performance-sensitive rendering, prefer a real GPU backend (WGPU, Vulkan, D3D12, OpenGL).
