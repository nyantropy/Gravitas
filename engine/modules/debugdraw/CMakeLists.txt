add_library(gravitas_debugdraw INTERFACE)

target_compile_features(gravitas_debugdraw INTERFACE cxx_std_20)

target_include_directories(gravitas_debugdraw INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/primitives"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
)

target_link_libraries(gravitas_debugdraw INTERFACE
    gravitas_core
    gravitas_rendering
)
