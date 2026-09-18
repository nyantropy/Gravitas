add_library(gravitas_model_runtime STATIC
    GtsModelResource.cpp
    GtsModelRegistry.cpp
    GtsModelSourceLoading.cpp
    GtsModelCookedLoading.cpp
)
target_link_libraries(gravitas_model_runtime PUBLIC gravitas_assets gravitas_cooked_assets PRIVATE gravitas_gltf_importer gravitas_obj_importer)
target_compile_features(gravitas_model_runtime PUBLIC cxx_std_20)
