cmake_minimum_required(VERSION 3.16)
project(UnitTests LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
# Populate Catch2 sources but do not build its library by default
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
  FetchContent_Populate(catch2)
endif()
# Add Catch2 with EXCLUDE_FROM_ALL so its targets are not built unless explicitly referenced
add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR} EXCLUDE_FROM_ALL)

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
    test_audio.cpp
    test_audio_playback.cpp
    test_subsystems.cpp
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

target_include_directories(UnitTests PRIVATE
  ${CMAKE_SOURCE_DIR}/Engine/Core/include
  ${CMAKE_SOURCE_DIR}/tests/include
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

# Copy runtime DLLs to test output directory on Windows
if (WIN32)
    if (NOT DEFINED VCPKG_INSTALLED)
        set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")
    endif()

    set(TEST_RUNTIME_DLLS
        "${VCPKG_INSTALLED}/bin/SDL2.dll"
        "${VCPKG_INSTALLED}/bin/assimp-vc143-mt.dll"
        "${VCPKG_INSTALLED}/bin/draco.dll"
        "${VCPKG_INSTALLED}/bin/kubazip.dll"
        "${VCPKG_INSTALLED}/bin/minizip.dll"
        "${VCPKG_INSTALLED}/bin/poly2tri.dll"
        "${VCPKG_INSTALLED}/bin/pugixml.dll"
        "${VCPKG_INSTALLED}/bin/zlib1.dll"
        "${VCPKG_INSTALLED}/bin/webgpu_dawn.dll"
        "${VCPKG_INSTALLED}/bin/abseil_dll.dll"
    )

    # Optionally include miniaudio runtime DLL if present (some builds produce a DLL)
    if (WIN32)
      if (EXISTS "${VCPKG_INSTALLED}/bin/miniaudio.dll")
        list(APPEND TEST_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/miniaudio.dll")
      endif()
      if (EXISTS "${CMAKE_BINARY_DIR}/_deps/miniaudio-build/Debug/miniaudio.dll")
        list(APPEND TEST_RUNTIME_DLLS "${CMAKE_BINARY_DIR}/_deps/miniaudio-build/Debug/miniaudio.dll")
      endif()
    endif()

    # Attempt to include MSVC redistributable CRT DLLs (if present on developer machine)
    if (MSVC)
        if (DEFINED ENV{ProgramFiles})
            file(GLOB VCRUNTIME_DIRS "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT")
            foreach(d ${VCRUNTIME_DIRS})
                file(GLOB CRT_DLLS "${d}/*.dll")
                list(APPEND TEST_RUNTIME_DLLS ${CRT_DLLS})
            endforeach()
        endif()
    endif()

    add_custom_command(TARGET UnitTests POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${TEST_RUNTIME_DLLS} $<TARGET_FILE_DIR:UnitTests>
        COMMENT "Copying runtime DLLs to test output directory"
    )
endif()
