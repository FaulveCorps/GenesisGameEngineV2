cmake_minimum_required(VERSION 3.16)
project(EngineCore LANGUAGES CXX)

# Determine vcpkg installed directory based on the selected triplet or environment
if (NOT DEFINED VCPKG_INSTALLED)
  if (DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}" CACHE PATH "vcpkg installed directory")
  else()
    # Default to x64-windows for local development convenience when not otherwise specified
    set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")
  endif()
endif()

# Prefer SDL3, fallback to SDL2
set(SDL_INCLUDE_DIR "")

if (EXISTS "${VCPKG_INSTALLED}/include/SDL3")
  set(SDL_INCLUDE_DIR "${VCPKG_INSTALLED}/include/SDL3")
  find_library(SDL_LIBRARY NAMES SDL3 PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
endif()

if (NOT SDL_LIBRARY AND EXISTS "${VCPKG_INSTALLED}/include/SDL2")
  set(SDL_INCLUDE_DIR "${VCPKG_INSTALLED}/include/SDL2")
  find_library(SDL_LIBRARY NAMES SDL2 PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
endif()

# Final attempt: prefer concrete lib files from vcpkg if present
if (EXISTS "${VCPKG_INSTALLED}/lib/SDL3.lib")
  set(SDL_LIBRARY "${VCPKG_INSTALLED}/lib/SDL3.lib")
elseif (EXISTS "${VCPKG_INSTALLED}/lib/SDL2.lib")
  set(SDL_LIBRARY "${VCPKG_INSTALLED}/lib/SDL2.lib")
elseif (NOT SDL_LIBRARY)
  find_library(SDL_LIBRARY NAMES SDL3 SDL2 PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
endif()

if (NOT SDL_LIBRARY)
  message(FATAL_ERROR "Could not find SDL2 or SDL3 in ${VCPKG_INSTALLED}/lib (install sdl2 or sdl3 via vcpkg)")
endif()

# OpenGL
find_package(OpenGL REQUIRED)

# GLM
find_package(glm CONFIG REQUIRED)

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

FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG docking
)
FetchContent_MakeAvailable(imgui)

# ImGuizmo
# We vendor ImGuizmo because we apply editor-specific behavior tweaks (view-cube interaction,
# face labels, etc). Using FetchContent would overwrite those customizations.
set(imguizmo_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/vendor/ImGuizmo")

add_library(EngineCore STATIC
    src/Engine.cpp
    src/Window.cpp
    src/OpenGLRenderer.cpp
    src/DirectXRenderer.cpp
    src/VulkanRenderer.cpp
    src/D3D12Renderer.cpp
    src/WgpuRenderer.cpp
    src/SoftwareRenderer.cpp
    src/RendererManager.cpp
    src/MeshRegistry.cpp
    src/ShaderRegistry.cpp
    src/TextureRegistry.cpp
    src/Texture.cpp
    src/GraphicsFactory.cpp
    src/Model.cpp
    src/Mesh.cpp
    src/Shader.cpp
    src/Scene.cpp
    src/DebugRenderer.cpp
    src/Profiler.cpp
    src/Stats.cpp
    src/ImGuiLayer.cpp
    src/PluginManager.cpp
    src/SubsystemRegistry.cpp
    src/SubsystemManager.cpp
    src/NullAudio.cpp
    src/NullShaderSubsystem.cpp
    src/GLShaderSubsystem.cpp
    src/NullPhysics.cpp
    src/BulletPhysics.cpp
    src/Box2DPhysics.cpp
    src/NullInput.cpp
    src/SDLInput.cpp
    src/NullNetwork.cpp
    src/NullSave.cpp
    src/FileSave.cpp
    src/NullScripting.cpp
    src/LuaScripting.cpp
    src/ModManager.cpp
    src/EnetNetwork.cpp
    src/MiniaudioAudio.cpp
    src/miniaudio_impl.cpp
    src/stb_image_impl.cpp
    src/WasmHostBindings.cpp
    src/WasmRuntime.cpp
    src/SceneLoader.cpp
    src/AnimationSystem.cpp
    src/UISystem.cpp
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl3.cpp
    ${imguizmo_SOURCE_DIR}/ImGuizmo.cpp
)  

target_compile_definitions(EngineCore PUBLIC GENESIS_ENABLE_OPENGL=1)

# Ensure preferred SDL include is searched early (prefer SDL2 when present)
if (DEFINED SDL_INCLUDE_DIR)
  include_directories(BEFORE ${SDL_INCLUDE_DIR})
endif()

target_include_directories(EngineCore PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
  ${entt_SOURCE_DIR}/single_include
  ${SDL_INCLUDE_DIR}
  ${ASSIMP_INCLUDE_DIR}
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
  ${imguizmo_SOURCE_DIR}
) 

target_compile_features(EngineCore PUBLIC cxx_std_17)

target_link_libraries(EngineCore PUBLIC ${SDL_LIBRARY} OpenGL::GL ${ASSIMP_LIBRARY} glm::glm)

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

# Lua (optional) - scripting backend
find_path(LUA_INCLUDE_DIR NAMES lua.hpp lua.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH)
if (LUA_INCLUDE_DIR)
  find_library(LUA_LIBRARY NAMES lua53 lua54 lua lua5.3 lua5.4 PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  if (LUA_LIBRARY)
    message(STATUS "Lua found: include=${LUA_INCLUDE_DIR} library=${LUA_LIBRARY}")
    target_include_directories(EngineCore PUBLIC ${LUA_INCLUDE_DIR})
    target_link_libraries(EngineCore PUBLIC ${LUA_LIBRARY})
    target_compile_definitions(EngineCore PUBLIC HAVE_LUA=1)
  else()
    message(STATUS "Lua headers found but library missing; Lua backend will be disabled")
  endif()
else()
  message(STATUS "Lua not found; Lua backend will be disabled at runtime")
endif()

# Box2D (optional) - 2D physics
# Note: Current vcpkg 'box2d' port provides Box2D v3 which is API-incompatible with
# the engine's Box2D v2-based implementation. To avoid breakage, Box2D support is
# disabled by default and can be enabled explicitly with -DENABLE_BOX2D=ON.
option(ENABLE_BOX2D "Enable Box2D physics backend (requires compatible Box2D v2.x)" OFF)
if (ENABLE_BOX2D)
  find_path(BOX2D_INCLUDE_DIR NAMES box2d/box2d.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH)
  if (BOX2D_INCLUDE_DIR)
    find_library(BOX2D_LIBRARY NAMES box2d Box2D PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
    if (BOX2D_LIBRARY)
      message(STATUS "Box2D found: include=${BOX2D_INCLUDE_DIR}")
      target_include_directories(EngineCore PUBLIC ${BOX2D_INCLUDE_DIR})
      target_link_libraries(EngineCore PUBLIC ${BOX2D_LIBRARY})
      target_compile_definitions(EngineCore PUBLIC HAVE_BOX2D=1)
    else()
      message(STATUS "Box2D headers found but library missing; Box2D backend will be disabled")
    endif()
  else()
    message(STATUS "Box2D not found; Box2D backend will be disabled at runtime")
  endif()
else()
  message(STATUS "Box2D support is disabled by default; pass -DENABLE_BOX2D=ON to enable if you have a compatible Box2D v2.x installation")
endif()

# wasm3 (optional) - lightweight WASM runtime for sandboxed mods
# Prefer vcpkg-provided wasm3 if available; otherwise WASM mod support will be disabled.
find_path(WASM3_INCLUDE_DIR NAMES wasm3.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH)
if (WASM3_INCLUDE_DIR)
  find_library(WASM3_LIBRARY NAMES wasm3 PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  if (WASM3_LIBRARY)
    message(STATUS "wasm3 found: include=${WASM3_INCLUDE_DIR} lib=${WASM3_LIBRARY}")
    target_include_directories(EngineCore PUBLIC ${WASM3_INCLUDE_DIR})
    target_link_libraries(EngineCore PUBLIC ${WASM3_LIBRARY})
    target_compile_definitions(EngineCore PUBLIC HAVE_WASM3=1)
  else()
    message(STATUS "wasm3 headers found but library missing; WASM mod support disabled")
  endif()
else()
  message(STATUS "wasm3 not found; WASM-based mod support will be disabled")
endif()

# ENet (optional) - lightweight UDP-based networking
find_path(ENET_INCLUDE_DIR NAMES enet/enet.h enet.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH)
if (ENET_INCLUDE_DIR)
  find_library(ENET_LIBRARY NAMES enet PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  if (ENET_LIBRARY)
    message(STATUS "ENet found: include=${ENET_INCLUDE_DIR}")
    target_compile_definitions(EngineCore PRIVATE HAVE_ENET=1)
    target_include_directories(EngineCore PRIVATE ${ENET_INCLUDE_DIR})
    target_link_libraries(EngineCore PRIVATE ${ENET_LIBRARY})
  else()
    message(STATUS "ENet headers found but library missing; ENet backend will be disabled")
  endif()
else()
  message(STATUS "ENet not found; ENet backend will be disabled at runtime")
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
