cmake_minimum_required(VERSION 3.16)
project(SampleGame LANGUAGES CXX)

add_executable(SampleGame
    main.cpp
)

target_link_libraries(SampleGame PRIVATE EngineCore)

target_compile_features(SampleGame PUBLIC cxx_std_17)
