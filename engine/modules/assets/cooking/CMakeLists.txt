add_library(gravitas_asset_cooking STATIC
    AssetCooker.cpp
    TextureCooker.cpp
    legacy/GltfAssetImporter.cpp
    legacy/ImageAssetImporter.cpp
)
target_link_libraries(gravitas_asset_cooking
    PUBLIC gravitas_cooked_assets
    PRIVATE gravitas_image_processing gravitas_obj_importer gravitas_gltf_source gravitas_image_decode
            gravitas_static_model_realization gravitas_static_geometry
)
target_compile_features(gravitas_asset_cooking PUBLIC cxx_std_20)
