function(add_gravitas_module_smoke_test name)
    add_test(
        NAME "gravitas_module_${name}"
        COMMAND "${CMAKE_COMMAND}"
            "-DGRAVITAS_SOURCE_DIR=${GTS_ENGINE_ROOT}"
            "-DGRAVITAS_BINARY_ROOT=${CMAKE_BINARY_DIR}/module-smoke"
            "-DGRAVITAS_SMOKE_CASE=${name}"
            "-DGRAVITAS_SMOKE_GENERATOR=${CMAKE_GENERATOR}"
            -P "${GTS_ENGINE_ROOT}/cmake/RunModuleSmokeTest.cmake"
    )
    set_tests_properties("gravitas_module_${name}"
        PROPERTIES
            LABELS "gravitas;modules;smoke"
    )
endfunction()

add_gravitas_module_smoke_test(core_only)
add_gravitas_module_smoke_test(rendering_without_backend)
add_gravitas_module_smoke_test(physics_without_rendering)
add_gravitas_module_smoke_test(vulkan_backend_no_tools)
add_gravitas_module_smoke_test(debugdraw_without_physics_is_rejected)
