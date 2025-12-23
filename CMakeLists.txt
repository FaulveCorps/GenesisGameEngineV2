cmake_minimum_required(VERSION 3.16)
# Prefer local vcpkg toolchain automatically if present (must be set before project())
if (EXISTS "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" AND NOT DEFINED CMAKE_TOOLCHAIN_FILE)
  set(CMAKE_TOOLCHAIN_FILE "${CMAKE_SOURCE_DIR}/vcpkg/scripts/buildsystems/vcpkg.cmake" CACHE STRING "Vcpkg toolchain file")
endif()
project(GenesisGameEngine VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Add engine core and sample game
add_subdirectory(Engine/Core)
add_subdirectory(GameProjects/SampleGame)
add_subdirectory(Tools)
add_subdirectory(Plugins/SamplePlugin)
add_subdirectory(tests)

include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "GenesisGameEngine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

# Install targets
install(TARGETS SampleGame
    RUNTIME DESTINATION bin
)
install(TARGETS SamplePlugin
    RUNTIME DESTINATION plugins
    LIBRARY DESTINATION plugins
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)

include(CPack)
