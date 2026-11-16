# Header-only material/resource services; no aggregate rendering or backend dependency.
# These services now compile their implementations; the authoring surface stays backend-independent.
add_library(gravitas_material_frontend STATIC
    authoring/MaterialAssetRealization.cpp
    authoring/MaterialRuntime.cpp
    authoring/MaterialReferenceHelpers.cpp
    internal/MaterialRuntimeAccess.cpp
    internal/MaterialRuntimeRegistry.cpp
    internal/SharedUnlitMaterialCache.cpp
)
target_compile_features(gravitas_material_frontend PUBLIC cxx_std_20)
target_link_libraries(gravitas_material_frontend PUBLIC
    gravitas_core gravitas_cooked_assets gravitas_rendering_contracts gravitas_image_decode)
target_include_directories(gravitas_material_frontend
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/authoring"
    PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/internal" "${CMAKE_CURRENT_SOURCE_DIR}/../interface")

add_library(gravitas_material_runtime_access INTERFACE)
target_link_libraries(gravitas_material_runtime_access INTERFACE gravitas_material_frontend)
target_include_directories(gravitas_material_runtime_access INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/internal" "${CMAKE_CURRENT_SOURCE_DIR}/../interface")
