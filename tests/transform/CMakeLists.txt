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
