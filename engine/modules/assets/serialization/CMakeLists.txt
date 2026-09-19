add_library(gravitas_cooked_assets STATIC AssetSerializers.cpp CookedAssetEncoding.cpp)
target_link_libraries(gravitas_cooked_assets PUBLIC gravitas_asset_contracts)
target_include_directories(gravitas_cooked_assets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/../loading/cooked" "${CMAKE_CURRENT_SOURCE_DIR}/../loading")
target_compile_features(gravitas_cooked_assets PUBLIC cxx_std_20)
