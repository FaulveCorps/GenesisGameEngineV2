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
    SRC/Engine.cpp
    SRC/Window.cpp
    SRC/OpenGLRenderer.cpp
    SRC/DirectXRenderer.cpp
    SRC/VulkanRenderer.cpp
    SRC/D3D12Renderer.cpp
    SRC/WgpuRenderer.cpp
    SRC/SoftwareRenderer.cpp
    SRC/RendererManager.cpp
    SRC/MeshRegistry.cpp
    SRC/ShaderRegistry.cpp
    SRC/TextureRegistry.cpp
    SRC/Texture.cpp
    SRC/GraphicsFactory.cpp
    SRC/Model.cpp
    SRC/Mesh.cpp
    SRC/Shader.cpp
    SRC/Scene.cpp
    SRC/Profiler.cpp
    SRC/Stats.cpp
    SRC/ImGuiLayer.cpp
    SRC/PluginManager.cpp
    SRC/SubsystemRegistry.cpp
    SRC/SubsystemManager.cpp
    SRC/NullAudio.cpp
    SRC/NullShaderSubsystem.cpp
    SRC/GLShaderSubsystem.cpp
    SRC/NullPhysics.cpp
    SRC/BulletPhysics.cpp
    SRC/MiniaudioAudio.cpp
    SRC/miniaudio_impl.cpp
    SRC/stb_image_impl.cpp
)  

target_include_directories(EngineCore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${entt_SOURCE_DIR}/single_include
  ${SDL2_INCLUDE_DIR}
  ${ASSIMP_INCLUDE_DIR}
)

target_compile_features(EngineCore PUBLIC cxx_std_17)

target_link_libraries(EngineCore PRIVATE ${SDL2_LIBRARY} OpenGL::GL ${ASSIMP_LIBRARY})

# stb single-file image library (optional) - fetch via FetchContent
FetchContent_Declare(
  stb
  GIT_REPOSITORY https://github.com/nothings/stb.git
  GIT_TAG master
)
FetchContent_MakeAvailable(stb)
# Expose the stb headers so we can use stb_image/stb_image_write
if (stb_SOURCE_DIR)
  target_include_directories(EngineCore PUBLIC ${stb_SOURCE_DIR})
  target_compile_definitions(EngineCore PUBLIC HAVE_STB_IMAGE=1)
endif()

# Miniaudio (optional) - single-header audio library for simple playback
FetchContent_Declare(
  miniaudio
  GIT_REPOSITORY https://github.com/mackron/miniaudio.git
  GIT_TAG master
)
FetchContent_MakeAvailable(miniaudio)
# Expose miniaudio header and mark availability
if (miniaudio_SOURCE_DIR)
  target_include_directories(EngineCore PUBLIC ${miniaudio_SOURCE_DIR})
  target_compile_definitions(EngineCore PUBLIC HAVE_MINIAUDIO=1)
endif()

# Vulkan (optional) - prefer system/vcpkg-provided Vulkan
find_package(Vulkan)
if (Vulkan_FOUND)
  message(STATUS "Vulkan found: ${Vulkan_VERSION}")
  # Expose HAVE_VULKAN to C++ sources so we can guard Vulkan headers
  target_compile_definitions(EngineCore PRIVATE HAVE_VULKAN=1)
  target_link_libraries(EngineCore PRIVATE Vulkan::Vulkan)
else()
  message(STATUS "Vulkan not found; VulkanRenderer will be disabled at runtime")
endif()

# Bullet (optional) - physics library
find_path(BULLET_INCLUDE_DIR NAMES btBulletDynamicsCommon.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH)
if (BULLET_INCLUDE_DIR)
  # Try to find common Bullet libs (BulletDynamics, BulletCollision, LinearMath)
  find_library(BULLET_DYNAMICS_LIB NAMES BulletDynamics PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(BULLET_COLLISION_LIB NAMES BulletCollision PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(BULLET_LINEARMATH_LIB NAMES LinearMath PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)

  if (BULLET_DYNAMICS_LIB AND BULLET_COLLISION_LIB AND BULLET_LINEARMATH_LIB)
    message(STATUS "Bullet found: include=${BULLET_INCLUDE_DIR}")
    target_compile_definitions(EngineCore PRIVATE HAVE_BULLET=1)
    target_include_directories(EngineCore PRIVATE ${BULLET_INCLUDE_DIR})
    target_link_libraries(EngineCore PRIVATE ${BULLET_DYNAMICS_LIB} ${BULLET_COLLISION_LIB} ${BULLET_LINEARMATH_LIB})
  else()
    message(STATUS "Bullet headers found but libraries missing; Bullet backend will be disabled")
  endif()
else()
  message(STATUS "Bullet not found; BulletPhysics will be disabled at runtime")
endif()

# bgfx support intentionally removed from this build (was optional)
# Previously the project attempted to find and enable bgfx; removal simplifies dependencies and maintenance.
# MSVC: prevent concurrent PDB write conflicts in parallel builds
if (MSVC)
  target_compile_options(EngineCore PRIVATE /FS)
endif()

# WGPU / Dawn (optional) - prefer vcpkg-provided Dawn or wgpu-native
# Try to detect common header/library names in vcpkg-installed include/lib
find_path(WGPU_INCLUDE_DIR NAMES wgpu.h webgpu.h dawn/dawn_proc.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH)
find_library(WGPU_LIBRARY NAMES dawn wgpu webgpu wgpu_native webgpu_dawn PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
# Fallback: check common vcpkg layout directly if find_* didn't match
if (NOT WGPU_INCLUDE_DIR)
  if (EXISTS "${VCPKG_INSTALLED}/include/webgpu/webgpu.h")
    set(WGPU_INCLUDE_DIR "${VCPKG_INSTALLED}/include")
  endif()
endif()
if (NOT WGPU_LIBRARY)
  if (EXISTS "${VCPKG_INSTALLED}/lib/webgpu_dawn.lib")
    set(WGPU_LIBRARY "${VCPKG_INSTALLED}/lib/webgpu_dawn.lib")
  endif()
endif()

if (WGPU_INCLUDE_DIR AND WGPU_LIBRARY)
  message(STATUS "WGPU/Dawn found: include=${WGPU_INCLUDE_DIR} lib=${WGPU_LIBRARY}")
  target_compile_definitions(EngineCore PUBLIC HAVE_WGPU=1)
  target_include_directories(EngineCore PRIVATE ${WGPU_INCLUDE_DIR})
  target_link_libraries(EngineCore PRIVATE ${WGPU_LIBRARY})
else()
  message(STATUS "WGPU/Dawn not found; WgpuRenderer will be disabled at runtime")
endif()

# Windows: link D3D12 and DXGI libraries for D3D12 renderer
if (WIN32)
  target_link_libraries(EngineCore PRIVATE d3d12 dxgi)
endif()
