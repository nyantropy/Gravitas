
# Shared CPU decoder implementation; rendering and cooking use the same stb symbols.
add_library(gravitas_image_decode STATIC StbImageImplementation.cpp image/GtsImageDecode.cpp)
target_include_directories(gravitas_image_decode PRIVATE "${GTS_ENGINE_ROOT}/external/stb")
target_compile_features(gravitas_image_decode PUBLIC cxx_std_20)

target_include_directories(gravitas_image_decode PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/image")
