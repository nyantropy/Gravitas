# Header-only material/resource services; no aggregate rendering or backend dependency.
add_library(gravitas_material_frontend INTERFACE)
target_link_libraries(gravitas_material_frontend INTERFACE gravitas_core gravitas_cooked_assets gravitas_rendering_contracts)
target_include_directories(gravitas_material_frontend INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../interface")

target_link_libraries(gravitas_material_frontend INTERFACE gravitas_image_decode)
