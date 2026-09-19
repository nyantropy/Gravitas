# Shared CPU asset infrastructure. Model phases live in modules/model.
add_subdirectory(importer)
add_subdirectory(serialization)
add_subdirectory(loading/mesh)
add_subdirectory(cooking)

add_library(gravitas_asset_contracts INTERFACE)
target_include_directories(gravitas_asset_contracts INTERFACE
    "${CMAKE_CURRENT_SOURCE_DIR}/geometry"
    "${CMAKE_CURRENT_SOURCE_DIR}/material"
)
target_link_libraries(gravitas_asset_contracts INTERFACE gravitas_core)
