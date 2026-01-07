cmake_minimum_required(VERSION 3.16)

if (NOT DEFINED VCPKG_INSTALLED)
  if (DEFINED VCPKG_TARGET_TRIPLET)
    set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/${VCPKG_TARGET_TRIPLET}" CACHE PATH "vcpkg installed directory")
  else()
    # Default to x64-windows for local development convenience when not otherwise specified
    set(VCPKG_INSTALLED "${CMAKE_SOURCE_DIR}/vcpkg/installed/x64-windows" CACHE PATH "vcpkg installed directory")
  endif()
endif()

add_executable(GenesisRuntime src/main.cpp)

target_include_directories(GenesisRuntime PRIVATE "${CMAKE_SOURCE_DIR}/Engine/Core/include")

target_link_libraries(GenesisRuntime PRIVATE EngineCore)

if(WIN32)
    # Find SDL2 and SDL2main libraries manually (similar to logic in Engine/Core)
    find_library(SDL2_LIB NAMES SDL2 PATHS "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
    find_library(SDL2MAIN_LIB NAMES SDL2main PATHS "${VCPKG_INSTALLED}/lib/manual-link" "${VCPKG_INSTALLED}/lib" NO_DEFAULT_PATH)
    
    if (SDL2_LIB AND SDL2MAIN_LIB)
        target_link_libraries(GenesisRuntime PRIVATE ${SDL2MAIN_LIB} ${SDL2_LIB})
    else()
        message(FATAL_ERROR "Could not find SDL2 or SDL2main in ${VCPKG_INSTALLED}/lib - GenesisRuntime requires SDL2")
    endif()
endif()

set_target_properties(GenesisRuntime PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)
