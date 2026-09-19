add_library(gravitas_runtime_execution INTERFACE)
target_compile_features(gravitas_runtime_execution INTERFACE cxx_std_20)
target_include_directories(gravitas_runtime_execution INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gravitas_runtime_execution INTERFACE
    gravitas_core
    gravitas_rendering_execution_contracts
    gravitas_vn_execution_contracts
)
