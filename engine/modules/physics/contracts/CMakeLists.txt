# Public physics access and collision values, without physics or transform implementation.
add_library(gravitas_physics_contracts INTERFACE)
target_compile_features(gravitas_physics_contracts INTERFACE cxx_std_20)
target_include_directories(gravitas_physics_contracts INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gravitas_physics_contracts INTERFACE gravitas_core)
