# Running the Sample Game

After building the project, run the SampleGame executable found in the build output (typically `Build/GameProjects/SampleGame/` or in the Visual Studio project output). The app will:
- Create a window using SDL3
- Initialize a minimal OpenGL renderer
- Load `Assets/models/triangle.obj` and render it
- Present an ImGui overlay showing FPS, frame time, and draw calls

Controls
- Close the window to exit the application.

Notes
- The SampleGame will attempt to load the sample plugin (`SamplePlugin`) from the working directory. On Windows the library will be `SamplePlugin.dll`; on POSIX `libSamplePlugin.so`.
