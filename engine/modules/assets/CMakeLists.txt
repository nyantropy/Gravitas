# Canonical CPU domain. Source importers and rendering-profile processing remain
# separate targets within this module so domain users need neither dependency.
add_library(gravitas_assets STATIC
    model/GtsModelImportResult.cpp
    model/GtsModelValidation.cpp
)
target_link_libraries(gravitas_assets PUBLIC gravitas_core)
target_include_directories(gravitas_assets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
target_compile_features(gravitas_assets PUBLIC cxx_std_20)

add_subdirectory(importer)
add_subdirectory(processing/geometry)
