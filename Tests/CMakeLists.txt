cmake_minimum_required(VERSION 3.16)
project(UnitTests LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(catch2)

add_executable(UnitTests
    test_core.cpp
    test_wgpu_smoke.cpp
    test_software_smoke.cpp
    test_renderer_switch.cpp
    test_shader_recreate.cpp
    test_texture_recreate.cpp
    test_texture_file.cpp
    test_texture_registry.cpp
    test_2d_sprite_smoke.cpp
    test_2d_sprite_gl_smoke.cpp
    test_2d_sprite_gl_readback.cpp
    test_samplegame_autocycle.cpp
    test_shader_subsystem.cpp
    test_pbr_material.cpp
    test_pbr_shader_compile.cpp
    test_renderer_mesh.cpp
    test_gltf_material_import.cpp
    test_audio.cpp
    test_audio_playback.cpp
    test_physics.cpp
    test_physics_rigidbody.cpp
    test_physics_box2d.cpp
    test_subsystems.cpp
    test_input.cpp
    test_input_sdl.cpp
    test_input_gamepad.cpp
    test_network.cpp
    test_network_enet.cpp
    test_save.cpp
    test_mods.cpp
    test_scripting.cpp
    test_wasm_runtime.cpp
    test_wasm_runtime_raii.cpp
    test_wasm_host_bindings.cpp
    test_wasm_deferred_unreg.cpp
    test_wasm_runtime.cpp
    test_wasm_sample_mod.cpp
    test_wasm_mod_file.cpp
    test_wasm_host_bindings.cpp
    test_wasm_e2e.cpp
    test_wasm_limits.cpp
    test_animation.cpp
    test_ui.cpp
    ${catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp
)

add_executable(gl_vaorepro
    gl_vaorepro.cpp
)

target_include_directories(gl_vaorepro PRIVATE
  ${CMAKE_SOURCE_DIR}/Engine/Core/include
)

target_link_libraries(gl_vaorepro PRIVATE EngineCore)

target_compile_features(gl_vaorepro PUBLIC cxx_std_17)

# Small smoke executable to validate WASM e2e on CI (runs independently of UnitTests)
# Only add the WASM E2E smoke when wasm3 runtime is present in vcpkg (CI installs wasm3)
set(WASM3_LIB_PATH "")
if (EXISTS "${VCPKG_INSTALLED}/bin/wasm3.dll")
  set(WASM3_LIB_PATH "${VCPKG_INSTALLED}/bin/wasm3.dll")
elseif (EXISTS "${VCPKG_INSTALLED}/lib/wasm3.lib")
  set(WASM3_LIB_PATH "${VCPKG_INSTALLED}/lib/wasm3.lib")
elseif (EXISTS "${VCPKG_INSTALLED}/lib/libwasm3.so")
  set(WASM3_LIB_PATH "${VCPKG_INSTALLED}/lib/libwasm3.so")
elseif (EXISTS "${VCPKG_INSTALLED}/lib/libwasm3.dylib")
  set(WASM3_LIB_PATH "${VCPKG_INSTALLED}/lib/libwasm3.dylib")
endif()

if (WASM3_LIB_PATH)
  add_executable(WasmE2ESmoke
      wasm_e2e_check.cpp
  )

  target_include_directories(WasmE2ESmoke PRIVATE
    ${CMAKE_SOURCE_DIR}/Engine/Core/include
  )

  target_link_libraries(WasmE2ESmoke PRIVATE EngineCore)

  target_compile_features(WasmE2ESmoke PUBLIC cxx_std_17)
  # Provide PROJECT_SOURCE_DIR to the smoke executable so it can locate sample mod files
  target_compile_definitions(WasmE2ESmoke PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")
  add_test(NAME WasmE2ESmoke COMMAND WasmE2ESmoke)
else()
  message(STATUS "wasm3 not found in ${VCPKG_INSTALLED}; WasmE2ESmoke will be skipped locally.")
endif()

target_include_directories(UnitTests PRIVATE
  ${CMAKE_SOURCE_DIR}/Engine/Core/include
  ${CMAKE_SOURCE_DIR}/Tests/include
  ${catch2_SOURCE_DIR}/single_include
  ${catch2_SOURCE_DIR}/src
  ${catch2_SOURCE_DIR}/extras
)

target_link_libraries(UnitTests PRIVATE EngineCore)

target_compile_features(UnitTests PUBLIC cxx_std_17)

# Let tests find project source dir for locating assets
target_compile_definitions(UnitTests PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

include(CTest)
add_test(NAME UnitTests COMMAND UnitTests)

# Ensure the vcpkg installed directory is known (works across platforms and CI triplets)
if (NOT DEFINED VCPKG_INSTALLED)
  if (DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}" CACHE PATH "vcpkg installed directory")
  else()
    set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")
  endif()
endif()

# Collect runtime libraries (platform-specific names) that should be copied into the test output directory
set(TEST_RUNTIME_LIBS
    "${VCPKG_INSTALLED}/bin/SDL2.dll"
    "${VCPKG_INSTALLED}/bin/assimp-vc143-mt.dll"
    "${VCPKG_INSTALLED}/bin/draco.dll"
    "${VCPKG_INSTALLED}/bin/kubazip.dll"
    "${VCPKG_INSTALLED}/bin/minizip.dll"
    "${VCPKG_INSTALLED}/bin/poly2tri.dll"
    "${VCPKG_INSTALLED}/bin/pugixml.dll"
    "${VCPKG_INSTALLED}/bin/zlib1.dll"
)

if (EXISTS "${VCPKG_INSTALLED}/bin/webgpu_dawn.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/webgpu_dawn.dll")
endif()

if (EXISTS "${VCPKG_INSTALLED}/bin/abseil_dll.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/abseil_dll.dll")
endif()

# Optional Windows DLLs
if (EXISTS "${VCPKG_INSTALLED}/bin/miniaudio.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/miniaudio.dll")
endif()
if (EXISTS "${CMAKE_BINARY_DIR}/_deps/miniaudio-build/Debug/miniaudio.dll")
  list(APPEND TEST_RUNTIME_LIBS "${CMAKE_BINARY_DIR}/_deps/miniaudio-build/Debug/miniaudio.dll")
endif()
if (EXISTS "${VCPKG_INSTALLED}/bin/enet.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/enet.dll")
endif()
# Optional Lua DLLs
if (EXISTS "${VCPKG_INSTALLED}/bin/lua.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/lua.dll")
endif()
if (EXISTS "${VCPKG_INSTALLED}/bin/lua5.3.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/lua5.3.dll")
endif()
if (EXISTS "${VCPKG_INSTALLED}/bin/lua5.4.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/lua5.4.dll")
endif()
if (EXISTS "${VCPKG_INSTALLED}/bin/lua53.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/lua53.dll")
endif()
if (EXISTS "${VCPKG_INSTALLED}/bin/lua54.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/lua54.dll")
endif()
if (EXISTS "${VCPKG_INSTALLED}/bin/box2d.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/box2d.dll")
endif()
if (EXISTS "${VCPKG_INSTALLED}/bin/wasm3.dll")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/bin/wasm3.dll")
endif()

# Non-Windows: pick up shared libraries if present (lib*.so or *.dylib)
if (EXISTS "${VCPKG_INSTALLED}/lib/libwasm3.so")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/lib/libwasm3.so")
endif()
if (EXISTS "${VCPKG_INSTALLED}/lib/libwasm3.dylib")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/lib/libwasm3.dylib")
endif()
if (EXISTS "${VCPKG_INSTALLED}/lib/libbox2d.so")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/lib/libbox2d.so")
endif()
if (EXISTS "${VCPKG_INSTALLED}/lib/libbox2d.dylib")
  list(APPEND TEST_RUNTIME_LIBS "${VCPKG_INSTALLED}/lib/libbox2d.dylib")
endif()

# Attempt to include MSVC redistributable CRT DLLs (if present on developer machine)
if (MSVC)
    if (DEFINED ENV{ProgramFiles})
        file(GLOB VCRUNTIME_DIRS "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT")
        foreach(d ${VCRUNTIME_DIRS})
            file(GLOB CRT_DLLS "${d}/*.dll")
            list(APPEND TEST_RUNTIME_LIBS ${CRT_DLLS})
        endforeach()
    endif()
endif()

# Copy any discovered runtime libs into the UnitTests output directory
if (TEST_RUNTIME_LIBS)
    add_custom_command(TARGET UnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TEST_RUNTIME_LIBS} $<TARGET_FILE_DIR:UnitTests>
        COMMENT "Copying runtime libs to test output directory"
    )
endif()

# Optionally include Bullet shared libs if present in vcpkg to support runtime tests that rely on dynamic libs
if (EXISTS "${VCPKG_INSTALLED}/lib/libBulletDynamics.so")
  add_custom_command(TARGET UnitTests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VCPKG_INSTALLED}/lib/libBulletDynamics.so" $<TARGET_FILE_DIR:UnitTests>
  )
endif()
if (EXISTS "${VCPKG_INSTALLED}/lib/libBulletCollision.so")
  add_custom_command(TARGET UnitTests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VCPKG_INSTALLED}/lib/libBulletCollision.so" $<TARGET_FILE_DIR:UnitTests>
  )
endif()
if (EXISTS "${VCPKG_INSTALLED}/lib/libLinearMath.so")
  add_custom_command(TARGET UnitTests POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different "${VCPKG_INSTALLED}/lib/libLinearMath.so" $<TARGET_FILE_DIR:UnitTests>
  )
endif()
