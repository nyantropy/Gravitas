# Test the option contract independently of runtime's target-existence gate.
set(expect_complete_runtime OFF)
if(GTS_ENABLE_RENDERING AND GTS_ENABLE_VULKAN_BACKEND AND GTS_ENABLE_PHYSICS
        AND GTS_ENABLE_DEBUGDRAW AND GTS_ENABLE_TOOLS)
    set(expect_complete_runtime ON)
endif()
foreach(facade_target gravitas_runtime gravitas_engine GravitasEngine)
    if(expect_complete_runtime AND NOT TARGET ${facade_target})
        message(FATAL_ERROR "Complete configuration is missing ${facade_target}")
    elseif(NOT expect_complete_runtime AND TARGET ${facade_target})
        message(FATAL_ERROR "Reduced configuration advertises ${facade_target}")
    endif()
endforeach()
if(NOT TARGET gravitas_runtime_execution OR NOT TARGET gravitas_core)
    message(FATAL_ERROR "Independent foundation/policy targets must remain available")
endif()

if(expect_complete_runtime)
    add_executable(RuntimeFacadeContractTest RuntimeFacadeContractTest.cpp)
    target_link_libraries(RuntimeFacadeContractTest PRIVATE GravitasEngine)
    add_test(NAME runtime_facade_contract COMMAND RuntimeFacadeContractTest)
endif()

add_test(NAME runtime_facade_availability
    COMMAND "${CMAKE_COMMAND}"
        "-DENGINE_SOURCE=${GTS_ENGINE_ROOT}"
        "-DTEST_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}"
        "-DEXPECT_COMPLETE=${expect_complete_runtime}"
        "-DRENDERING=${GTS_ENABLE_RENDERING}"
        "-DVULKAN=${GTS_ENABLE_VULKAN_BACKEND}"
        "-DPHYSICS=${GTS_ENABLE_PHYSICS}"
        "-DDEBUGDRAW=${GTS_ENABLE_DEBUGDRAW}"
        "-DTOOLS=${GTS_ENABLE_TOOLS}"
        -P "${GTS_ENGINE_ROOT}/cmake/TestRuntimeAvailability.cmake")
