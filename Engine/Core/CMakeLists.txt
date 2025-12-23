cmake_minimum_required(VERSION 3.16)
project(EngineCore LANGUAGES CXX)

# Use CONFIG mode to find vcpkg-provided packages
find_package(SDL3 CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(assimp CONFIG REQUIRED)

include(FetchContent)
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG v3.11.0
)
FetchContent_MakeAvailable(entt)

add_library(EngineCore STATIC
    src/Engine.cpp
    src/Window.cpp
    src/OpenGLRenderer.cpp
    src/DirectXRenderer.cpp
    src/Model.cpp
    src/Mesh.cpp
    src/Shader.cpp
    src/Scene.cpp
    src/Profiler.cpp
    src/Stats.cpp
    src/ImGuiLayer.cpp
    src/PluginManager.cpp
)

target_include_directories(EngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${entt_SOURCE_DIR}/single_include
)

target_compile_features(EngineCore PUBLIC cxx_std_17)

target_link_libraries(EngineCore PRIVATE SDL3::SDL3 OpenGL::GL assimp::assimp)

target_link_libraries(EngineCore PRIVATE SDL3::SDL3)

# MSVC: prevent concurrent PDB write conflicts in parallel builds
if (MSVC)
  target_compile_options(EngineCore PRIVATE /FS)
endif()
