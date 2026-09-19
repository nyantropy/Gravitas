add_subdirectory(execution)

# Header-only composition root. Leaf modules must never link this target.
add_library(gravitas_runtime INTERFACE)
target_compile_features(gravitas_runtime INTERFACE cxx_std_20)
target_include_directories(gravitas_runtime INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gravitas_runtime INTERFACE
    gravitas_core
    gravitas_tool_contracts
    gravitas_modules
    gravitas_runtime_execution
)

if(TARGET gravitas_vulkan_backend)
    target_link_libraries(gravitas_runtime INTERFACE gravitas_vulkan_backend)
endif()
