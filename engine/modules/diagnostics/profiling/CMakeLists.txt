# Telemetry schema and its accumulator remain available in every configuration.
add_library(gravitas_profiling INTERFACE)
target_compile_features(gravitas_profiling INTERFACE cxx_std_20)
target_include_directories(gravitas_profiling INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
