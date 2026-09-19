add_library(gravitas_cooked_assets STATIC AssetSerializers.cpp CookedAssetEncoding.cpp)
target_link_libraries(gravitas_cooked_assets PUBLIC gravitas_core)
target_include_directories(gravitas_cooked_assets PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/../..")
target_compile_features(gravitas_cooked_assets PUBLIC cxx_std_20)
