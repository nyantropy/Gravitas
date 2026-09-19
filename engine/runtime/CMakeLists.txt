# Policy values remain usable by integration tests in reduced capability builds.
add_subdirectory(execution)

# GravitasEngine is the complete composition, not an arbitrary module subset.
set(runtime_requirements gravitas_rendering gravitas_vulkan_backend gravitas_physics
    gravitas_debugdraw gravitas_tools)
set(missing_runtime_requirements "")
foreach(capability IN LISTS runtime_requirements)
    if(NOT TARGET ${capability})
        list(APPEND missing_runtime_requirements ${capability})
    endif()
endforeach()
if(missing_runtime_requirements)
    message(STATUS "GravitasEngine unavailable (capability-development configuration); missing: ${missing_runtime_requirements}")
    return()
endif()

add_library(gravitas_runtime STATIC rendering/GraphicsBackendInstaller.cpp)
target_compile_features(gravitas_runtime PUBLIC cxx_std_20)
target_include_directories(gravitas_runtime PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gravitas_runtime PUBLIC
    gravitas_core
    gravitas_tool_contracts
    gravitas_modules
    gravitas_runtime_execution
    PRIVATE gravitas_vulkan_backend
)
