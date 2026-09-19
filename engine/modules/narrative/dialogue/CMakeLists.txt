add_library(gravitas_dialogue INTERFACE)
target_compile_features(gravitas_dialogue INTERFACE cxx_std_20)
target_include_directories(gravitas_dialogue INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/events"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
)
target_link_libraries(gravitas_dialogue INTERFACE gravitas_core)
