cmake_minimum_required(VERSION 3.16)
project(shader_compiler LANGUAGES CXX)

add_executable(shader_compiler main.cpp)

target_compile_features(shader_compiler PUBLIC cxx_std_17)

target_include_directories(shader_compiler PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
