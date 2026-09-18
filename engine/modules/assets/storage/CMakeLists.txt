# Existing cooked-v1 codecs are CPU-only despite their historical source location.
add_library(gravitas_cooked_assets STATIC
    ../../rendering/core/assets/AssetSerializers.cpp
)
target_link_libraries(gravitas_cooked_assets PUBLIC gravitas_core)
target_include_directories(gravitas_cooked_assets PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/../.."
    "${CMAKE_CURRENT_SOURCE_DIR}/../../rendering/core/assets"
    "${CMAKE_CURRENT_SOURCE_DIR}/../../rendering/core/material"
)
target_compile_features(gravitas_cooked_assets PUBLIC cxx_std_20)
