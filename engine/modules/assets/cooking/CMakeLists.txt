add_library(gravitas_asset_cooking STATIC
    AssetCooker.cpp
    TextureCooker.cpp
)
target_link_libraries(gravitas_asset_cooking
    PUBLIC gravitas_cooked_assets
    PRIVATE gravitas_image_processing gravitas_obj_importer gravitas_gltf_importer gravitas_image_decode
            gravitas_static_geometry
)
target_compile_features(gravitas_asset_cooking PUBLIC cxx_std_20)
