cmake_minimum_required(VERSION 3.16)
project(SampleGame LANGUAGES CXX)

add_executable(SampleGame
    main.cpp
)

target_link_libraries(SampleGame PRIVATE EngineCore)

target_compile_features(SampleGame PUBLIC cxx_std_17)

# Copy assets to the target build folder so SampleGame can load them at runtime
add_custom_command(TARGET SampleGame POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/assets ${CMAKE_CURRENT_BINARY_DIR}/assets
)

# Copy runtime DLLs (SDL3, Assimp and dependencies) into the SampleGame output folder on Windows
if (WIN32)
    if (NOT DEFINED VCPKG_INSTALLED)
        set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")
    endif()

    set(SAMPLEGAME_RUNTIME_DLLS
          "${VCPKG_INSTALLED}/bin/SDL2.dll"
        "${VCPKG_INSTALLED}/bin/assimp-vc143-mt.dll"
        "${VCPKG_INSTALLED}/bin/draco.dll"
        "${VCPKG_INSTALLED}/bin/kubazip.dll"
        "${VCPKG_INSTALLED}/bin/minizip.dll"
        "${VCPKG_INSTALLED}/bin/poly2tri.dll"
        "${VCPKG_INSTALLED}/bin/pugixml.dll"
        "${VCPKG_INSTALLED}/bin/zlib1.dll"
    )

    add_custom_command(TARGET SampleGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SampleGame>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SAMPLEGAME_RUNTIME_DLLS} $<TARGET_FILE_DIR:SampleGame>
        COMMENT "Copying runtime DLLs to SampleGame output directory"
    )

    # Copy SamplePlugin.dll to SampleGame output directory
    add_custom_command(TARGET SampleGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different 
            $<TARGET_FILE:SamplePlugin> 
            $<TARGET_FILE_DIR:SampleGame>
        COMMENT "Copying SamplePlugin.dll to SampleGame output directory"
    )
endif()
