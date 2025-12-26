cmake_minimum_required(VERSION 3.16)
project(SamplePlugin LANGUAGES CXX)

add_library(SamplePlugin SHARED SamplePlugin.cpp)

target_compile_features(SamplePlugin PUBLIC cxx_std_17)

target_include_directories(SamplePlugin PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Core/include)

# On Windows, export symbols in DLL
if (WIN32)
    target_compile_definitions(SamplePlugin PRIVATE PLUGIN_EXPORT)
endif()
