cmake_minimum_required(VERSION 3.16)
project(EngineCore LANGUAGES CXX)

# Manually set paths to vcpkg-installed packages
set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")

# SDL2 - headers under include/SDL2; code uses <SDL.h>
set(SDL2_INCLUDE_DIR "${VCPKG_INSTALLED}/include/SDL2")
find_library(SDL2_LIBRARY NAMES SDL2 PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH REQUIRED)

# OpenGL
find_package(OpenGL REQUIRED)

# Assimp - manually find 
find_path(ASSIMP_INCLUDE_DIR assimp/scene.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH REQUIRED)
find_library(ASSIMP_LIBRARY NAMES assimp-vc143-mt PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH REQUIRED)

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
  ${SDL2_INCLUDE_DIR}
  ${ASSIMP_INCLUDE_DIR}
)

target_compile_features(EngineCore PUBLIC cxx_std_17)

target_link_libraries(EngineCore PRIVATE ${SDL2_LIBRARY} OpenGL::GL ${ASSIMP_LIBRARY})

# MSVC: prevent concurrent PDB write conflicts in parallel builds
if (MSVC)
  target_compile_options(EngineCore PRIVATE /FS)
endif()
