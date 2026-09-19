# Resource identity only; available without rendering, windowing or a GPU backend.
add_library(gravitas_rendering_contracts INTERFACE)
target_compile_features(gravitas_rendering_contracts INTERFACE cxx_std_20)
target_include_directories(gravitas_rendering_contracts INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
