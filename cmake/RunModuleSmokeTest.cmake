if(NOT DEFINED GRAVITAS_SOURCE_DIR)
    message(FATAL_ERROR "GRAVITAS_SOURCE_DIR is required")
endif()

if(NOT DEFINED GRAVITAS_BINARY_ROOT)
    message(FATAL_ERROR "GRAVITAS_BINARY_ROOT is required")
endif()

if(NOT DEFINED GRAVITAS_SMOKE_CASE)
    message(FATAL_ERROR "GRAVITAS_SMOKE_CASE is required")
endif()

set(case_binary_dir "${GRAVITAS_BINARY_ROOT}/${GRAVITAS_SMOKE_CASE}")
set(expect_configure_failure OFF)
set(expected_configure_error "")

set(configure_args
    -S "${GRAVITAS_SOURCE_DIR}"
    -B "${case_binary_dir}"
    -DCMAKE_BUILD_TYPE=Debug
    -DGTS_BUILD_TEST_SCENES=OFF
    -DGTS_ENABLE_MODULE_SMOKE_TESTS=OFF
    -DGTS_ENABLE_RUNTIME_SMOKE_TESTS=OFF
)

if(DEFINED GRAVITAS_SMOKE_GENERATOR AND NOT GRAVITAS_SMOKE_GENERATOR STREQUAL "")
    list(APPEND configure_args -G "${GRAVITAS_SMOKE_GENERATOR}")
endif()

if(GRAVITAS_SMOKE_CASE STREQUAL "core_only")
    list(APPEND configure_args
        -DGTS_ENABLE_RENDERING=OFF
        -DGTS_ENABLE_VULKAN_BACKEND=OFF
        -DGTS_ENABLE_PHYSICS=OFF
        -DGTS_ENABLE_DEBUGDRAW=OFF
        -DGTS_ENABLE_TOOLS=OFF
    )
elseif(GRAVITAS_SMOKE_CASE STREQUAL "rendering_without_backend")
    list(APPEND configure_args
        -DGTS_ENABLE_RENDERING=ON
        -DGTS_ENABLE_VULKAN_BACKEND=OFF
        -DGTS_ENABLE_PHYSICS=ON
        -DGTS_ENABLE_DEBUGDRAW=OFF
        -DGTS_ENABLE_TOOLS=OFF
    )
elseif(GRAVITAS_SMOKE_CASE STREQUAL "physics_without_rendering")
    list(APPEND configure_args
        -DGTS_ENABLE_RENDERING=OFF
        -DGTS_ENABLE_VULKAN_BACKEND=OFF
        -DGTS_ENABLE_PHYSICS=ON
        -DGTS_ENABLE_DEBUGDRAW=OFF
        -DGTS_ENABLE_TOOLS=OFF
    )
elseif(GRAVITAS_SMOKE_CASE STREQUAL "vulkan_backend_no_tools")
    list(APPEND configure_args
        -DGTS_ENABLE_RENDERING=ON
        -DGTS_ENABLE_VULKAN_BACKEND=ON
        -DGTS_ENABLE_PHYSICS=ON
        -DGTS_ENABLE_DEBUGDRAW=OFF
        -DGTS_ENABLE_TOOLS=OFF
    )
elseif(GRAVITAS_SMOKE_CASE STREQUAL "debugdraw_without_physics_is_rejected")
    list(APPEND configure_args
        -DGTS_ENABLE_RENDERING=ON
        -DGTS_ENABLE_VULKAN_BACKEND=OFF
        -DGTS_ENABLE_PHYSICS=OFF
        -DGTS_ENABLE_DEBUGDRAW=ON
        -DGTS_ENABLE_TOOLS=OFF
    )
    set(expect_configure_failure ON)
    set(expected_configure_error "GTS_ENABLE_DEBUGDRAW currently requires GTS_ENABLE_PHYSICS")
else()
    message(FATAL_ERROR "Unknown Gravitas module smoke case: ${GRAVITAS_SMOKE_CASE}")
endif()

file(REMOVE_RECURSE "${case_binary_dir}")

execute_process(
    COMMAND "${CMAKE_COMMAND}" ${configure_args}
    RESULT_VARIABLE configure_result
    OUTPUT_VARIABLE configure_output
    ERROR_VARIABLE configure_error
)

set(configure_log "${configure_output}\n${configure_error}")

if(expect_configure_failure)
    if(configure_result EQUAL 0)
        message(FATAL_ERROR
            "Expected configure failure for ${GRAVITAS_SMOKE_CASE}, but configure succeeded")
    endif()

    if(expected_configure_error AND NOT configure_log MATCHES "${expected_configure_error}")
        message(FATAL_ERROR
            "Configure failed for ${GRAVITAS_SMOKE_CASE}, but not with the expected error.\n"
            "${configure_log}")
    endif()

    message(STATUS "Expected configure rejection observed for ${GRAVITAS_SMOKE_CASE}")
    return()
endif()

if(NOT configure_result EQUAL 0)
    message(FATAL_ERROR
        "Configure failed for ${GRAVITAS_SMOKE_CASE}.\n"
        "${configure_log}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${case_binary_dir}" --parallel
    RESULT_VARIABLE build_result
    OUTPUT_VARIABLE build_output
    ERROR_VARIABLE build_error
)

if(NOT build_result EQUAL 0)
    message(FATAL_ERROR
        "Build failed for ${GRAVITAS_SMOKE_CASE}.\n"
        "${build_output}\n${build_error}")
endif()

message(STATUS "Smoke build passed for ${GRAVITAS_SMOKE_CASE}")
