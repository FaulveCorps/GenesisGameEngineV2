cmake_minimum_required(VERSION 3.16)
project(GenesisGameEngine VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Add engine core and sample game
add_subdirectory(Engine/Core)
add_subdirectory(GameProjects/SampleGame)
add_subdirectory(Tools)
