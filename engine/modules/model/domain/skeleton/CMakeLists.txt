add_library(gravitas_skeleton_assets STATIC
    GtsSkeletonAsset.cpp
    GtsSkeletonCompatibility.cpp
    GtsSkeletonValidation.cpp
)
target_link_libraries(gravitas_skeleton_assets PUBLIC gravitas_core)
target_include_directories(gravitas_skeleton_assets PUBLIC "${GTS_ENGINE_ROOT}/engine/modules")
target_compile_features(gravitas_skeleton_assets PUBLIC cxx_std_20)
