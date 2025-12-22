cmake_minimum_required(VERSION 3.16)
project(asset_packer LANGUAGES CXX)

add_executable(asset_packer main.cpp)

target_compile_features(asset_packer PUBLIC cxx_std_17)

target_include_directories(asset_packer PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/../..)
