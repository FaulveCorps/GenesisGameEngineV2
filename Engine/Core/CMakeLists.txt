cmake_minimum_required(VERSION 3.16)
project(EngineCore LANGUAGES CXX)

find_package(SDL3 REQUIRED)
find_package(OpenGL REQUIRED)
find_package(assimp REQUIRED)

include(FetchContent)
FetchContent_Declare(
  entt
  GIT_REPOSITORY https://github.com/skypjack/entt.git
  GIT_TAG v3.11.0
)
FetchContent_MakeAvailable(entt)

# Fetch Dear ImGui
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.89.10
)
FetchContent_MakeAvailable(imgui)

add_library(imgui_static
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl3.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_opengl2.cpp
)

target_include_directories(imgui_static PUBLIC
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui_static PUBLIC SDL3::SDL3 OpenGL::GL)

add_library(EngineCore STATIC
    src/Engine.cpp
    src/Window.cpp
    src/OpenGLRenderer.cpp
    src/DirectXRenderer.cpp
    src/Model.cpp
    src/Mesh.cpp
    src/Shader.cpp
    src/Scene.cpp
    src/Profiler.cpp
    src/Stats.cpp
    src/ImGuiLayer.cpp
    src/PluginManager.cpp
)

target_include_directories(EngineCore PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${entt_SOURCE_DIR}/single_include
)

target_compile_features(EngineCore PUBLIC cxx_std_17)

target_link_libraries(EngineCore PRIVATE SDL3::SDL3 OpenGL::GL assimp::assimp imgui_static)

target_link_libraries(EngineCore PRIVATE SDL3::SDL3)
