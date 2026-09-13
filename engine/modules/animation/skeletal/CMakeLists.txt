add_library(gravitas_skeletal_animation STATIC
    runtime/GtsAnimationPlayback.cpp
    GtsAnimationSampling.cpp
    GtsSkeletonPoseEvaluation.cpp
    GtsSkeletonPoseValidation.cpp
)
target_link_libraries(gravitas_skeletal_animation PUBLIC gravitas_animation_assets)
target_include_directories(gravitas_skeletal_animation PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_features(gravitas_skeletal_animation PUBLIC cxx_std_20)
