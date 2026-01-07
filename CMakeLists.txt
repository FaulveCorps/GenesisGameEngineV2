cmake_minimum_required(VERSION 3.16)
project(GenesisGameEngine VERSION 0.1 LANGUAGES CXX)

# ============================================================================
# Compiler and Build Optimization Settings
# ============================================================================
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Enable all cores for parallel compilation
if(MSVC)
    # Parallel compilation: Allow multiple compiler processes to write to shared PDB
    add_compile_options(/FS)
    
    # Use embedded debug info in object files to reduce PDB contention
    # (each .obj file contains its own debug info, not shared in single .pdb)
    set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "Embedded")
    
    # Enable function-level linking for better optimization
    add_compile_options(/Gy)
    
    # Fast floating-point model
    add_compile_options(/fp:fast)
    
    # Inline function expansion (mode 2: any)
    add_compile_options(/Ob2)
endif()

# ============================================================================
# Build Configuration
# ============================================================================
enable_testing()

# Speed up CMake file operations
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ============================================================================
# Add Subdirectories
# ============================================================================
add_subdirectory(Engine/Core)
add_subdirectory(GameProjects/SampleGame)
add_subdirectory(Editor)
add_subdirectory(Tools)
add_subdirectory(Plugins/SamplePlugin)
add_subdirectory(tests)
add_subdirectory(Runtime)

# ============================================================================
# Installation and Packaging
# ============================================================================
include(InstallRequiredSystemLibraries)
set(CPACK_GENERATOR "ZIP")
set(CPACK_PACKAGE_NAME "GenesisGameEngine")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_SOURCE_DIR}/LICENSE")

install(TARGETS SampleGame
    RUNTIME DESTINATION bin
)
install(TARGETS SamplePlugin
    RUNTIME DESTINATION plugins
    LIBRARY DESTINATION plugins
)
install(DIRECTORY ${CMAKE_SOURCE_DIR}/assets DESTINATION .)

include(CPack)
