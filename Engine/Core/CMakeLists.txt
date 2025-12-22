cmake_minimum_required(VERSION 3.16)
project(EngineCore LANGUAGES CXX)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(assimp REQUIRED)

add_library(EngineCore STATIC
    src/Engine.cpp
    src/Window.cpp
    src/OpenGLRenderer.cpp
    src/Model.cpp
)

target_include_directories(EngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(EngineCore PUBLIC cxx_std_17)

target_link_libraries(EngineCore PRIVATE SDL3::SDL3 OpenGL::GL assimp::assimp)

target_link_libraries(EngineCore PRIVATE SDL3::SDL3)
