add_library(gravitas_visualnovel INTERFACE)
target_compile_features(gravitas_visualnovel INTERFACE cxx_std_20)
target_include_directories(gravitas_visualnovel INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/ui"
)
target_link_libraries(gravitas_visualnovel INTERFACE
    gravitas_vn_execution_contracts
    gravitas_dialogue
    gravitas_rendering
)
