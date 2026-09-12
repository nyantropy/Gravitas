add_library(gravitas_static_geometry STATIC GtsStaticMeshPreparation.cpp)
target_link_libraries(gravitas_static_geometry PUBLIC gravitas_assets)
target_include_directories(gravitas_static_geometry PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${GTS_ENGINE_ROOT}/engine/modules/rendering/core/geometry"
)
target_compile_features(gravitas_static_geometry PUBLIC cxx_std_20)
