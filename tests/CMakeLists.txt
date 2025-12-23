cmake_minimum_required(VERSION 3.16)
project(UnitTests LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
# Populate Catch2 sources but do not build its library by default
FetchContent_GetProperties(catch2)
if(NOT catch2_POPULATED)
  FetchContent_Populate(catch2)
endif()
# Add Catch2 with EXCLUDE_FROM_ALL so its targets are not built unless explicitly referenced
add_subdirectory(${catch2_SOURCE_DIR} ${catch2_BINARY_DIR} EXCLUDE_FROM_ALL)

add_executable(UnitTests
    test_core.cpp
    ${catch2_SOURCE_DIR}/extras/catch_amalgamated.cpp
)

target_include_directories(UnitTests PRIVATE
  ${CMAKE_SOURCE_DIR}/Engine/Core/include
  ${CMAKE_SOURCE_DIR}/tests/include
  ${catch2_SOURCE_DIR}/src
  ${catch2_SOURCE_DIR}/extras
)

target_link_libraries(UnitTests PRIVATE EngineCore)

target_compile_features(UnitTests PUBLIC cxx_std_17)

# Let tests find project source dir for locating assets
target_compile_definitions(UnitTests PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

include(CTest)
add_test(NAME UnitTests COMMAND UnitTests)
