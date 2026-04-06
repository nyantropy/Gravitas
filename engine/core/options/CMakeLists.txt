add_library(options INTERFACE)

target_include_directories(options INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_compile_definitions(options INTERFACE
    GRAVITAS_ENGINE_ROOT="${GTS_ENGINE_ROOT}"
    GRAVITAS_ENGINE_RESOURCES="${GTS_ENGINE_ROOT}/resources"
)
