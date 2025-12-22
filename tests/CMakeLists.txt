cmake_minimum_required(VERSION 3.16)
project(UnitTests LANGUAGES CXX)

include(FetchContent)
FetchContent_Declare(
  catch2
  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
  GIT_TAG v3.4.0
)
FetchContent_MakeAvailable(catch2)

add_executable(UnitTests
    test_core.cpp
)

target_include_directories(UnitTests PRIVATE ${CMAKE_SOURCE_DIR}/Engine/Core/include)

target_link_libraries(UnitTests PRIVATE EngineCore Catch2::Catch2WithMain)

target_compile_features(UnitTests PUBLIC cxx_std_17)

# Let tests find project source dir for locating assets
target_compile_definitions(UnitTests PRIVATE PROJECT_SOURCE_DIR="${CMAKE_SOURCE_DIR}")

include(CTest)
add_test(NAME UnitTests COMMAND UnitTests)
