# Post-Processing Pipeline

Genesis Engine includes a built-in post-processing pipeline in the OpenGL renderer. This pipeline handles:
- Tone Mapping (Reinhard)
- Gamma Correction
- Exposure Control

## Usage

The post-processing parameters can be controlled via the `IGraphicsAPI` interface.

```cpp
// Get the current renderer
auto renderer = Genesis::Engine::RendererManager::GetRenderer();

// Set exposure and gamma
// exposure: Controls the brightness of the scene (default 1.0)
// gamma: Controls the gamma correction (default 2.2)
renderer->SetPostProcessParams(1.5f, 2.2f);
```

## Implementation Details

The pipeline uses an offscreen framebuffer (FBO) to render the scene to a texture. This texture is then rendered to a full-screen quad using a post-process shader (`postprocess.vert` / `postprocess.frag`).

### Shaders

The shaders are located in `Assets/shaders/`:
- `postprocess.vert`: Passes vertex positions and texture coordinates.
- `postprocess.frag`: Performs tone mapping and gamma correction.

### Customization

You can modify `postprocess.frag` to add more effects like:
- Bloom
- Color Grading
- Vignette
- Chromatic Aberration

## Performance

The post-processing pass adds a small overhead (one full-screen draw call). It is automatically enabled when using the `OpenGLRenderer`.
```