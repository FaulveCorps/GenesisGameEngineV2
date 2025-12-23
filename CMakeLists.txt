cmake_minimum_required(VERSION 3.16)
project(GenesisGameEngine VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
# Work around MSVC PDB contention in parallel builds
if(MSVC)
    add_compile_options(/FS)
    # Use embedded debug info in object files to avoid shared PDB writes
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
endif()
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
