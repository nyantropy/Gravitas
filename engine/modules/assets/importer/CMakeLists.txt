add_subdirectory(obj)
add_subdirectory(gltf)

# Shared CPU decoder implementation; rendering and cooking use the same stb symbols.
add_library(gravitas_image_decode STATIC StbImageImplementation.cpp)
target_include_directories(gravitas_image_decode PUBLIC "${GTS_ENGINE_ROOT}/external/stb")
target_compile_features(gravitas_image_decode PUBLIC cxx_std_20)
