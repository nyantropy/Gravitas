add_library(options INTERFACE)

target_include_directories(options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(options INTERFACE
    GRAVITAS_ENGINE_ROOT="${CMAKE_SOURCE_DIR}"
    GRAVITAS_ENGINE_RESOURCES="${CMAKE_SOURCE_DIR}/resources"
)