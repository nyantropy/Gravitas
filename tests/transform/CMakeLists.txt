add_executable(TransformAnimationRuntimeTest ../animation/TransformAnimationRuntimeTest.cpp)
target_link_libraries(TransformAnimationRuntimeTest PRIVATE gravitas_transform_animation)
add_test(NAME transform_animation_runtime COMMAND TransformAnimationRuntimeTest)
set_tests_properties(transform_animation_runtime PROPERTIES LABELS "gravitas;animation;runtime")

add_executable(TransformHierarchyRuntimeTest
    TransformHierarchyRuntimeTest.cpp
)

target_compile_features(TransformHierarchyRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(TransformHierarchyRuntimeTest PRIVATE gravitas_transform)

add_test(
    NAME transform_hierarchy_runtime
    COMMAND TransformHierarchyRuntimeTest
)

set_tests_properties(transform_hierarchy_runtime
    PROPERTIES
        LABELS "gravitas;transform;runtime"
)

add_executable(TransformWorldLifetimeTest TransformWorldLifetimeTest.cpp)
target_link_libraries(TransformWorldLifetimeTest PRIVATE gravitas_transform)
add_test(NAME transform_world_lifetime COMMAND TransformWorldLifetimeTest)
set_tests_properties(transform_world_lifetime PROPERTIES LABELS "gravitas;transform;lifecycle;cpu")

add_executable(TransformInstallationTest TransformInstallationTest.cpp)
target_link_libraries(TransformInstallationTest PRIVATE gravitas_transform)
add_test(NAME transform_installation COMMAND TransformInstallationTest)
set_tests_properties(transform_installation PROPERTIES LABELS "gravitas;transform;runtime;cpu")

if(TARGET gravitas_tools)
    add_executable(TransformPreviewLifetimeTest TransformPreviewLifetimeTest.cpp)
    target_link_libraries(TransformPreviewLifetimeTest PRIVATE gravitas_tools gravitas_transform)
    add_test(NAME transform_preview_lifetime COMMAND TransformPreviewLifetimeTest)
    set_tests_properties(transform_preview_lifetime PROPERTIES LABELS "gravitas;transform;tools;lifecycle;cpu")
endif()
