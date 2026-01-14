cmake_minimum_required(VERSION 3.16)
project(GenesisEditor LANGUAGES CXX)

add_executable(GenesisEditor
    src/main.cpp
)

target_link_libraries(GenesisEditor PRIVATE EngineCore)

target_compile_features(GenesisEditor PUBLIC cxx_std_17)

# Copy assets to the target build folder
add_custom_command(TARGET GenesisEditor POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/Assets $<TARGET_FILE_DIR:GenesisEditor>/Assets
)

# Copy runtime DLLs (SDL3, Assimp and dependencies) into the output folder on Windows
if (WIN32)
    if (NOT DEFINED VCPKG_INSTALLED)
        set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")
    endif()

    set(EDITOR_RUNTIME_DLLS
      # Prefer SDL3. Keep SDL2 as well if present (some local setups still have it installed).
      "${VCPKG_INSTALLED}/bin/SDL3.dll"
        "${VCPKG_INSTALLED}/bin/assimp-vc143-mt.dll"
        "${VCPKG_INSTALLED}/bin/draco.dll"
        "${VCPKG_INSTALLED}/bin/kubazip.dll"
        "${VCPKG_INSTALLED}/bin/minizip.dll"
        "${VCPKG_INSTALLED}/bin/poly2tri.dll"
        "${VCPKG_INSTALLED}/bin/pugixml.dll"
        "${VCPKG_INSTALLED}/bin/zlib1.dll"
    )

    if (EXISTS "${VCPKG_INSTALLED}/bin/SDL2.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/SDL2.dll")
    endif()

    if (EXISTS "${VCPKG_INSTALLED}/bin/webgpu_dawn.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/webgpu_dawn.dll")
    endif()

    if (EXISTS "${VCPKG_INSTALLED}/bin/abseil_dll.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/abseil_dll.dll")
    endif()
    
    # Optional DLLs
    if (EXISTS "${VCPKG_INSTALLED}/bin/miniaudio.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/miniaudio.dll")
    endif()
    if (EXISTS "${VCPKG_INSTALLED}/bin/enet.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/enet.dll")
    endif()
    if (EXISTS "${VCPKG_INSTALLED}/bin/lua.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/lua.dll")
    endif()
    if (EXISTS "${VCPKG_INSTALLED}/bin/box2d.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/box2d.dll")
    endif()
    if (EXISTS "${VCPKG_INSTALLED}/bin/wasm3.dll")
      list(APPEND EDITOR_RUNTIME_DLLS "${VCPKG_INSTALLED}/bin/wasm3.dll")
    endif()

    # Attempt to include MSVC redistributable CRT DLLs (if present on developer machine)
    if (MSVC)
        if (DEFINED ENV{ProgramFiles})
            file(GLOB VCRUNTIME_DIRS "$ENV{ProgramFiles}/Microsoft Visual Studio/2022/*/VC/Redist/MSVC/*/x64/Microsoft.VC143.CRT")
            foreach(d ${VCRUNTIME_DIRS})
                file(GLOB CRT_DLLS "${d}/*.dll")
                list(APPEND EDITOR_RUNTIME_DLLS ${CRT_DLLS})
            endforeach()
        endif()
    endif()

    add_custom_command(TARGET GenesisEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:GenesisEditor>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${EDITOR_RUNTIME_DLLS} $<TARGET_FILE_DIR:GenesisEditor>
        COMMENT "Copying runtime DLLs to GenesisEditor output directory"
    )
    
    # Copy SamplePlugin.dll to Editor output directory (optional, but good for testing)
    add_custom_command(TARGET GenesisEditor POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            $<TARGET_FILE:SamplePlugin> 
            $<TARGET_FILE_DIR:GenesisEditor>
        COMMENT "Copying SamplePlugin.dll to GenesisEditor output directory"
    )
endif()

  # CTest integration: headless regression self-test for the unsaved-changes quit/close prompt.
  # NOTE: Use $<TARGET_FILE_DIR:GenesisEditor> as working directory so copied runtime DLLs are found.
  include(CTest)
  if (BUILD_TESTING)
    add_test(
      NAME GenesisEditor_QuitPromptSelftest
      COMMAND $<TARGET_FILE:GenesisEditor> --quit-prompt-selftest
    )
    set_tests_properties(GenesisEditor_QuitPromptSelftest PROPERTIES
      WORKING_DIRECTORY $<TARGET_FILE_DIR:GenesisEditor>
    )
  endif()
