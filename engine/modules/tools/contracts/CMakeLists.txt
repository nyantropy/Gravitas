# Startup values are usable even when the tool implementation is disabled.
add_library(gravitas_tool_contracts STATIC
    EngineToolQueries.cpp ToolCameraHandoff.cpp ToolLaunchPreset.cpp)
target_compile_features(gravitas_tool_contracts PUBLIC cxx_std_20)
target_include_directories(gravitas_tool_contracts PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_include_directories(gravitas_tool_contracts PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/../core" "${CMAKE_CURRENT_SOURCE_DIR}/../cameras")
target_link_libraries(gravitas_tool_contracts PUBLIC gravitas_core)
