# Prepared VN selections are opaque core values, without coordinated engine policy.
add_library(gravitas_vn_execution_contracts INTERFACE)
target_compile_features(gravitas_vn_execution_contracts INTERFACE cxx_std_20)
target_include_directories(gravitas_vn_execution_contracts INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(gravitas_vn_execution_contracts INTERFACE gravitas_core)
