add_library(gravitas_transform STATIC
    runtime/TransformInvalidationLifecycle.cpp
    scene/TransformSceneFeature.cpp
    systems/TransformSystem.cpp
    systems/TransformWorldResolver.cpp
)
target_compile_features(gravitas_transform PUBLIC cxx_std_20)
target_include_directories(gravitas_transform PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/hierarchy"
    "${CMAKE_CURRENT_SOURCE_DIR}/runtime"
    "${CMAKE_CURRENT_SOURCE_DIR}/scene"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
)
target_link_libraries(gravitas_transform PUBLIC gravitas_core)
