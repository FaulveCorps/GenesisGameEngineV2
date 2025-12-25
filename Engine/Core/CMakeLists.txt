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
    src/VulkanRenderer.cpp
    src/D3D12Renderer.cpp
    src/BgfxRenderer.cpp
    src/WgpuRenderer.cpp
    src/GraphicsFactory.cpp
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

# bgfx (optional) - prefer vcpkg-provided bgfx
find_path(BGFX_INCLUDE_DIR NAMES bgfx/bgfx.h PATHS "${VCPKG_INSTALLED}/include" NO_DEFAULT_PATH)
find_library(BGFX_LIBRARY NAMES bgfx PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
if (BGFX_INCLUDE_DIR AND BGFX_LIBRARY)
  message(STATUS "bgfx found: include=${BGFX_INCLUDE_DIR} lib=${BGFX_LIBRARY}")
  # Only enable BUILD-level HAVE_BGFX for non-Debug builds (avoids linking release bgfx libs into Debug builds)
  target_compile_definitions(EngineCore PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:HAVE_BGFX=1>
  )
  target_include_directories(EngineCore PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:${BGFX_INCLUDE_DIR}>
  )
  # Link bgfx only for non-Debug builds to avoid CRT/iterator mismatches with vcpkg release libs
  target_link_libraries(EngineCore PRIVATE
    $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:${BGFX_LIBRARY}>
  )

  # bgfx has additional dependencies (bx, bimg, and optionally bimg_decode/encode, lodepng, miniz, squish, tinyexr)
  find_library(BX_LIBRARY NAMES bx PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(BIMG_LIBRARY NAMES bimg PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(BIMG_DECODE_LIBRARY NAMES bimg_decode PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(BIMG_ENCODE_LIBRARY NAMES bimg_encode PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(LODEPNG_LIBRARY NAMES lodepng liblodepng lodepng-c PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(MINIZ_LIBRARY NAMES miniz PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(SQUISH_LIBRARY NAMES squish PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
  find_library(TINYEXR_LIBRARY NAMES tinyexr PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)

  set(BGFX_DEP_LIBS)
  if(BX_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${BX_LIBRARY})
  endif()
  if(BIMG_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${BIMG_LIBRARY})
  endif()
  if(BIMG_DECODE_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${BIMG_DECODE_LIBRARY})
  endif()
  if(BIMG_ENCODE_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${BIMG_ENCODE_LIBRARY})
  endif()
  if(LODEPNG_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${LODEPNG_LIBRARY})
  endif()
  if(MINIZ_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${MINIZ_LIBRARY})
  endif()
  if(SQUISH_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${SQUISH_LIBRARY})
  endif()
  if(TINYEXR_LIBRARY)
    list(APPEND BGFX_DEP_LIBS ${TINYEXR_LIBRARY})
  endif()

  if(BGFX_DEP_LIBS)
    message(STATUS "bgfx dependencies found: ${BGFX_DEP_LIBS}")
    # Link bgfx dependencies only for non-Debug builds to avoid CRT/iterator mismatches
    target_link_libraries(EngineCore PRIVATE
      $<$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>,$<CONFIG:MinSizeRel>>:${BGFX_DEP_LIBS}>
    )

  endif()
else()
  message(STATUS "bgfx not found; BgfxRenderer will be disabled at runtime")
endif()

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
