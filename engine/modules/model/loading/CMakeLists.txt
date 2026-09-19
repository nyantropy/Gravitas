add_library(gravitas_model_loading STATIC
    GtsModelResource.cpp
    GtsModelRegistry.cpp
    GtsModelSourceLoading.cpp
    GtsModelCookedLoading.cpp
)
target_link_libraries(gravitas_model_loading PUBLIC gravitas_model_import gravitas_model_serialization PRIVATE gravitas_gltf_importer gravitas_obj_importer)
target_compile_features(gravitas_model_loading PUBLIC cxx_std_20)
