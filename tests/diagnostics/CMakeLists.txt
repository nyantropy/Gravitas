get_target_property(module_links gravitas_modules INTERFACE_LINK_LIBRARIES)
if(GTS_ENABLE_DEBUGDRAW)
    if(NOT TARGET gravitas_debugdraw OR NOT "gravitas_debugdraw" IN_LIST module_links)
        message(FATAL_ERROR "Enabled debug drawing must be built and aggregated")
    endif()
else()
    if(TARGET gravitas_debugdraw OR "gravitas_debugdraw" IN_LIST module_links)
        message(FATAL_ERROR "Disabled debug drawing must not be built or aggregated")
    endif()
endif()

if(GTS_ENABLE_PHYSICS AND GTS_ENABLE_DEBUGDRAW)
    if(NOT TARGET gravitas_diagnostics_physics OR NOT "gravitas_diagnostics_physics" IN_LIST module_links)
        message(FATAL_ERROR "Physics diagnostics must be built and aggregated when both features are enabled")
    endif()
else()
    if(TARGET gravitas_diagnostics_physics OR "gravitas_diagnostics_physics" IN_LIST module_links)
        message(FATAL_ERROR "Physics diagnostics requires both physics and debug drawing")
    endif()
endif()

if(TARGET gravitas_debugdraw)
    add_executable(DebugDrawRuntimeTest DebugDrawRuntimeTest.cpp)
    target_link_libraries(DebugDrawRuntimeTest PRIVATE gravitas_debugdraw)
    add_test(NAME debugdraw_runtime COMMAND DebugDrawRuntimeTest)
    set_tests_properties(debugdraw_runtime PROPERTIES LABELS "gravitas;diagnostics;cpu")
endif()

if(TARGET gravitas_diagnostics_physics)
    add_executable(PhysicsDebugDrawRuntimeTest PhysicsDebugDrawRuntimeTest.cpp)
    target_link_libraries(PhysicsDebugDrawRuntimeTest PRIVATE gravitas_diagnostics_physics)
    add_test(NAME physics_debugdraw_runtime COMMAND PhysicsDebugDrawRuntimeTest)
    set_tests_properties(physics_debugdraw_runtime PROPERTIES LABELS "gravitas;diagnostics;physics;cpu")
endif()

add_executable(ProfileAccumulatorTest
    ProfileAccumulatorTest.cpp
)

target_compile_features(ProfileAccumulatorTest PRIVATE cxx_std_20)

target_link_libraries(ProfileAccumulatorTest PRIVATE gravitas_profiling)

add_test(
    NAME profile_accumulator
    COMMAND ProfileAccumulatorTest
)

set_tests_properties(profile_accumulator
    PROPERTIES
        LABELS "gravitas;diagnostics;profiling"
)
