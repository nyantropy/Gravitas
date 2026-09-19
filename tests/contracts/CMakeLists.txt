add_executable(PhysicsContractsTest PhysicsContractsTest.cpp)
target_link_libraries(PhysicsContractsTest PRIVATE gravitas_physics_contracts)
add_test(NAME physics_contracts COMMAND PhysicsContractsTest)

add_executable(RenderingContractsTest RenderingContractsTest.cpp)
target_link_libraries(RenderingContractsTest PRIVATE gravitas_rendering_contracts)
add_test(NAME rendering_contracts COMMAND RenderingContractsTest)

set_tests_properties(physics_contracts rendering_contracts PROPERTIES LABELS "gravitas;contracts;cpu")

add_executable(RenderingControllerContextTest RenderingControllerContextTest.cpp)
target_link_libraries(RenderingControllerContextTest PRIVATE gravitas_rendering_controller_contracts)
add_test(NAME rendering_controller_context COMMAND RenderingControllerContextTest)

add_executable(ModelControllerContextTest ModelControllerContextTest.cpp)
target_link_libraries(ModelControllerContextTest PRIVATE gravitas_model_controller_contracts)
add_test(NAME model_controller_context COMMAND ModelControllerContextTest)

add_executable(ModelControllerIntegrationTest ModelControllerIntegrationTest.cpp)
target_link_libraries(ModelControllerIntegrationTest PRIVATE gravitas_model_frontend)
add_test(NAME model_controller_integration COMMAND ModelControllerIntegrationTest)

add_executable(ExecutionPolicyTest ExecutionPolicyTest.cpp)
target_link_libraries(ExecutionPolicyTest PRIVATE gravitas_runtime_execution gravitas_vn_execution_contracts)
add_test(NAME execution_policy COMMAND ExecutionPolicyTest)

if(TARGET gravitas_rendering)
    add_executable(EnginePauseTest EnginePauseTest.cpp)
    target_link_libraries(EnginePauseTest PRIVATE gravitas_runtime_execution)
    target_link_libraries(EnginePauseTest PRIVATE gravitas_runtime)
    add_test(NAME engine_pause_execution COMMAND EnginePauseTest)
endif()

foreach(boundary_case allowed core_modules core_runtime modules_runtime transitive_alias
        include_directory root_include_directory core_source_include module_source_include
        core_relative_include module_runtime_source)
    add_test(NAME source_layers_${boundary_case}
        COMMAND "${CMAKE_COMMAND}"
            "-DCHECK_FILE=${GTS_ENGINE_ROOT}/cmake/GravitasSourceLayers.cmake"
            "-DFIXTURE_ROOT=${CMAKE_CURRENT_BINARY_DIR}/source_layer_fixtures"
            "-DCASE=${boundary_case}"
            -P "${GTS_ENGINE_ROOT}/cmake/TestSourceLayers.cmake")
endforeach()

add_executable(ModuleBoundaryTest ModuleBoundaryTest.cpp)
target_link_libraries(ModuleBoundaryTest PRIVATE gravitas_transform gravitas_rendering_contracts)
add_test(NAME module_boundary COMMAND ModuleBoundaryTest)

add_executable(ModuleExecutionInputsTest ModuleExecutionInputsTest.cpp)
target_link_libraries(ModuleExecutionInputsTest PRIVATE gravitas_transform_animation)
if(TARGET gravitas_rendering)
    target_compile_definitions(ModuleExecutionInputsTest PRIVATE TEST_RENDERING_INPUTS)
    target_link_libraries(ModuleExecutionInputsTest PRIVATE gravitas_visualnovel)
endif()
if(TARGET gravitas_physics)
    target_compile_definitions(ModuleExecutionInputsTest PRIVATE TEST_PHYSICS_INPUTS)
    target_link_libraries(ModuleExecutionInputsTest PRIVATE gravitas_physics)
endif()
add_test(NAME module_execution_inputs COMMAND ModuleExecutionInputsTest)

if(TARGET gravitas_rendering)
    add_executable(PlatformInputTest ../runtime/PlatformInputTest.cpp)
    target_link_libraries(PlatformInputTest PRIVATE gravitas_runtime)
    add_test(NAME platform_input COMMAND PlatformInputTest)
    set_tests_properties(platform_input PROPERTIES LABELS "gravitas;input;runtime;cpu")
endif()

add_executable(ScreenshotCommandTest ScreenshotCommandTest.cpp)
target_link_libraries(ScreenshotCommandTest PRIVATE gravitas_rendering_command_contracts)
add_test(NAME screenshot_command COMMAND ScreenshotCommandTest)
set_tests_properties(screenshot_command PROPERTIES LABELS "gravitas;contracts;screenshot;cpu")

add_executable(MaterialWorldLifetimeTest MaterialWorldLifetimeTest.cpp)
target_link_libraries(MaterialWorldLifetimeTest PRIVATE gravitas_material_frontend)
add_test(NAME material_world_lifetime COMMAND MaterialWorldLifetimeTest)
set_tests_properties(material_world_lifetime PROPERTIES LABELS "gravitas;rendering;lifecycle;cpu")

if(TARGET gravitas_rendering AND TARGET gravitas_tools)
    add_executable(RendererWorldLifetimeTest ../rendering/RendererWorldLifetimeTest.cpp)
    target_link_libraries(RendererWorldLifetimeTest PRIVATE gravitas_rendering gravitas_tools gravitas_runtime_execution)
    add_test(NAME renderer_world_lifetime COMMAND RendererWorldLifetimeTest)
    add_test(NAME renderer_world_static_shutdown COMMAND RendererWorldLifetimeTest --static-shutdown)
    set_tests_properties(renderer_world_lifetime renderer_world_static_shutdown
        PROPERTIES LABELS "gravitas;rendering;lifecycle;tools;cpu")
endif()
