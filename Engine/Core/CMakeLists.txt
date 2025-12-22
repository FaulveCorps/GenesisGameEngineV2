cmake_minimum_required(VERSION 3.16)
project(EngineCore LANGUAGES CXX)

add_library(EngineCore STATIC
    src/Engine.cpp
)

target_include_directories(EngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(EngineCore PUBLIC cxx_std_17)
