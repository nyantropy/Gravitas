add_library(gravitas_rendering_window_contracts INTERFACE)
target_link_libraries(gravitas_rendering_window_contracts INTERFACE gravitas_core)
target_include_directories(gravitas_rendering_window_contracts INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../init"
)
