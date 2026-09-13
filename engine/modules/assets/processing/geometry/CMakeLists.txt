add_library(gravitas_primitive_geometry STATIC GtsPrimitiveGeometryPreparation.cpp)
target_link_libraries(gravitas_primitive_geometry PUBLIC gravitas_assets)
target_include_directories(gravitas_primitive_geometry PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GTS_ENGINE_ROOT}/engine/modules/rendering/core/geometry"
)
target_compile_features(gravitas_primitive_geometry PUBLIC cxx_std_20)

add_subdirectory(static)
add_subdirectory(skinned)
