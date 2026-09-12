add_library(gravitas_skin_assets STATIC GtsSkinBindingValidation.cpp)
target_link_libraries(gravitas_skin_assets PUBLIC gravitas_skeleton_assets)
target_include_directories(gravitas_skin_assets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_features(gravitas_skin_assets PUBLIC cxx_std_20)
