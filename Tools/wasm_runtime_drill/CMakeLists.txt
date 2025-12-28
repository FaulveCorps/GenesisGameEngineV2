add_executable(wasm_runtime_drill wasm_runtime_drill.cpp)
target_link_libraries(wasm_runtime_drill PRIVATE EngineCore)
# Provide PROJECT_SOURCE_DIR to the executable so the tool can access repo files
target_compile_definitions(wasm_runtime_drill PRIVATE PROJECT_SOURCE_DIR=\"${CMAKE_SOURCE_DIR}\")
set_target_properties(wasm_runtime_drill PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools/Debug")
