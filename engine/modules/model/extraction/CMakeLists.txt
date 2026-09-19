# End of model semantics: immutable renderer-ready frame work.
add_library(gravitas_model_frontend STATIC GtsModelRenderExtraction.cpp)
target_link_libraries(gravitas_model_frontend PUBLIC gravitas_model_world)
target_compile_features(gravitas_model_frontend PUBLIC cxx_std_20)

target_include_directories(gravitas_model_frontend PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(gravitas_model_frontend PUBLIC gravitas_transform)
