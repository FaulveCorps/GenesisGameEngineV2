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

# Copy runtime DLLs (SDL3, Assimp) into the SampleGame output folder on Windows
if (WIN32)
    if (NOT DEFINED VCPKG_INSTALLED)
        set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")
    endif()

    set(SAMPLEGAME_RUNTIME_DLLS
        "${VCPKG_INSTALLED}/bin/SDL3.dll"
        "${VCPKG_INSTALLED}/bin/assimp-vc143-mt.dll"
    )

    add_custom_command(TARGET SampleGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory $<TARGET_FILE_DIR:SampleGame>
        COMMAND ${CMAKE_COMMAND} -E copy_if_different ${SAMPLEGAME_RUNTIME_DLLS} $<TARGET_FILE_DIR:SampleGame>
    )
endif()
