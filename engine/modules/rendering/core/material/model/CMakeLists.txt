# Material frontend only: available to headless tests without renderer/backend libraries.
add_library(gravitas_model_materials STATIC GtsModelMaterialRealization.cpp)
target_link_libraries(gravitas_model_materials PUBLIC gravitas_model_realization PRIVATE gravitas_image_processing)
target_include_directories(gravitas_model_materials PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../interface")
target_compile_features(gravitas_model_materials PUBLIC cxx_std_20)
