# Genesis Game Engine — Architecture

This document gives a high-level overview of the engine architecture used in the sprint prototype.

Core components:
- Engine/Core
  - Windowing & Input: SDL3 wrapper (`Window`) provides SDL window and GL context.
  - Rendering: `IGraphicsAPI` (OpenGLRenderer) — small abstraction so backends can be swapped.
  - Audio/Physics (planned): placeholder for future integrations (OpenAL, Bullet).
  - ECS: `entt` is used for the registry and components; `Scene` aggregates entities and systems.
  - Asset loading: `Model` uses Assimp for model import; supports triangulation and immediate-mode draw for the sample.
  - Tools: `asset_packer` and `shader_compiler` in `Tools/`.

Plugins:
- A versioned `PluginAPI.h` is provided; `PluginManager` can load plugin shared objects at runtime.

Notes:
- The current renderer is minimal and uses OpenGL fixed-function / immediate-mode for simplicity. Future work should modernize to VAO/VBO/shaders.
- Build system: CMake with FetchContent usage for third-party headers (EnTT, ImGui). Runtime dependencies (SDL3, Assimp) are provided via vcpkg in CI or system packages.
