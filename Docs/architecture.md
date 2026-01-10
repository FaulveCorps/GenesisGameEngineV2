# Genesis Game Engine — Architecture

This document gives a high-level overview of the engine architecture used in the sprint prototype.

Core components:
- Engine/Core
  - Windowing & Input: SDL3 wrapper (`Window`) provides SDL window and GL context.
  - Rendering: `IGraphicsAPI` (OpenGLRenderer, VulkanRenderer, etc.) — abstraction layer for multiple backends.
    - **OpenGLRenderer**: Modern OpenGL 3.3+ pipeline featuring:
      - **Deferred Rendering**: G-Buffer pass (Position, Normal, Albedo/Spec) and Deferred Lighting pass.
      - **PBR**: Physically Based Rendering workflow.
      - **Shadow Mapping**: Directional light shadows.
      - **Post-Processing**: Bloom (Gaussian blur), Tone Mapping (Exposure), and Gamma Correction.
      - **glTF 2.0**: Full support for PBR materials and textures.
    - **VulkanRenderer**: Experimental backend.
  - Audio: Miniaudio integration.
  - Physics: Bullet (3D) and Box2D (2D) integration.
  - ECS: `entt` is used for the registry and components; `Scene` aggregates entities and systems.
  - Asset loading: `Model` uses Assimp for model import (glTF 2.0, OBJ); `Texture` uses stb_image.
  - Scene Management: `SceneLoader` supports data-driven scene loading from `.scene` files.
  - Scripting: Lua and WASM (WebAssembly) support.
  - Networking: ENet integration.
  - Tools: `asset_packer` and `shader_compiler` in `Tools/`.

Plugins:
- A versioned `PluginAPI.h` is provided; `PluginManager` can load plugin shared objects at runtime.

Notes:
- The renderer has been modernized to use a PBR pipeline with post-processing and **Shader Hot-Reloading**.
- Build system: CMake with FetchContent usage for third-party headers (EnTT, ImGui). Runtime dependencies (SDL3, Assimp) are provided via vcpkg in CI or system packages.
