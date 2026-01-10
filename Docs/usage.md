# Running the Sample Game

After building the project, run the SampleGame executable found in the build output (typically `Build/GameProjects/SampleGame/` or in the Visual Studio project output). The app will:
- Create a window using SDL3
- Initialize the renderer (OpenGL by default)
- Load `Assets/models/pbr_sample.gltf` (PBR model) or fallback to `triangle.obj`
- Render the scene with PBR lighting and post-processing (Tone Mapping, Gamma Correction)
- Present an ImGui overlay showing FPS, frame time, and draw calls

## Controls
- **F2**: Cycle through available renderers (OpenGL, Vulkan, etc.)
- **F5**: Save game state
- **F6**: Load game state
- Close the window to exit the application.

## Post-Processing
The engine includes a post-processing pipeline. See [post_processing.md](post_processing.md) for details.

## Notes
- The SampleGame will attempt to load the sample plugin (`SamplePlugin`) from the working directory. On Windows the library will be `SamplePlugin.dll`; on POSIX `libSamplePlugin.so`.

