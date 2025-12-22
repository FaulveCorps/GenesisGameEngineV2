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
