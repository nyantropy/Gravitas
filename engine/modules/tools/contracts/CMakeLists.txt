# Startup values are usable even when the tool implementation is disabled.
add_library(gravitas_tool_contracts INTERFACE)
target_compile_features(gravitas_tool_contracts INTERFACE cxx_std_20)
target_include_directories(gravitas_tool_contracts INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
