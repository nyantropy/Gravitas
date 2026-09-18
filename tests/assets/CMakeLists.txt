add_executable(GtsModelAssetTest GtsModelAssetTest.cpp)
target_link_libraries(GtsModelAssetTest PRIVATE gravitas_assets)
add_test(NAME gts_model_asset COMMAND GtsModelAssetTest)
set_tests_properties(gts_model_asset PROPERTIES LABELS "gravitas;assets;cpu")

add_executable(GtsModelSkinTest GtsModelSkinTest.cpp)
target_link_libraries(GtsModelSkinTest PRIVATE gravitas_assets)
add_test(NAME gts_model_skin COMMAND GtsModelSkinTest)
set_tests_properties(gts_model_skin PROPERTIES LABELS "gravitas;assets;cpu")

add_executable(GtsModelMaterialTest GtsModelMaterialTest.cpp)
target_link_libraries(GtsModelMaterialTest PRIVATE gravitas_assets)
add_test(NAME gts_model_material COMMAND GtsModelMaterialTest)
set_tests_properties(gts_model_material PROPERTIES LABELS "gravitas;assets;cpu")

add_subdirectory(importers/obj)
add_executable(GtsModelImportBundleTest importers/GtsModelImportBundleTest.cpp)
target_link_libraries(GtsModelImportBundleTest PRIVATE gravitas_assets)
add_test(NAME gts_model_import_bundle COMMAND GtsModelImportBundleTest)
set_tests_properties(gts_model_import_bundle PROPERTIES LABELS "gravitas;assets;cpu;importer")
add_subdirectory(processing/geometry)
add_subdirectory(importers/gltf)
add_subdirectory(skeleton)
add_subdirectory(skin)
add_subdirectory(animation)

add_executable(GtsModelRegistryTest runtime/GtsModelRegistryTest.cpp)
target_link_libraries(GtsModelRegistryTest PRIVATE gravitas_model_runtime gravitas_gltf_importer)
add_test(NAME gts_model_registry COMMAND GtsModelRegistryTest)
set_tests_properties(gts_model_registry PROPERTIES LABELS "gravitas;assets;runtime;cpu")

add_executable(GtsModelLoadingTest runtime/GtsModelLoadingTest.cpp)
target_link_libraries(GtsModelLoadingTest PRIVATE gravitas_model_runtime gravitas_gltf_importer)
add_test(NAME gts_model_loading COMMAND GtsModelLoadingTest)
set_tests_properties(gts_model_loading PROPERTIES LABELS "gravitas;assets;runtime;cpu")

add_executable(GtsModelStrictPolicyTest runtime/GtsModelStrictPolicyTest.cpp)
target_link_libraries(GtsModelStrictPolicyTest PRIVATE gravitas_model_runtime)
add_test(NAME gts_model_strict_policy COMMAND GtsModelStrictPolicyTest)
set_tests_properties(gts_model_strict_policy PROPERTIES LABELS "gravitas;assets;runtime;cpu")

add_executable(GltfAssetImporterTest
    GltfAssetImporterTest.cpp
)

target_compile_features(GltfAssetImporterTest PRIVATE cxx_std_20)

target_link_libraries(GltfAssetImporterTest PRIVATE gravitas_asset_cooking)

add_test(
    NAME gltf_asset_importer
    COMMAND GltfAssetImporterTest
)

set_tests_properties(gltf_asset_importer
    PROPERTIES
        LABELS "gravitas;assets;cpu;importer"
)

add_executable(AssetSerializationTest
    AssetSerializationTest.cpp
)

target_compile_features(AssetSerializationTest PRIVATE cxx_std_20)

target_link_libraries(AssetSerializationTest PRIVATE gravitas_cooked_assets)

add_test(
    NAME asset_serialization
    COMMAND AssetSerializationTest
)

set_tests_properties(asset_serialization
    PROPERTIES
        LABELS "gravitas;assets;cpu;serialization"
)

add_executable(RuntimeAssetPolicyTest
    RuntimeAssetPolicyTest.cpp
)

target_compile_features(RuntimeAssetPolicyTest PRIVATE cxx_std_20)

target_link_libraries(RuntimeAssetPolicyTest PRIVATE gravitas_mesh_loading)

add_test(
    NAME runtime_asset_policy
    COMMAND RuntimeAssetPolicyTest
)

set_tests_properties(runtime_asset_policy
    PROPERTIES
        LABELS "gravitas;assets;cpu;runtime"
)

add_executable(GtsModelRealizationTest realization/GtsModelRealizationTest.cpp)
target_link_libraries(GtsModelRealizationTest PRIVATE gravitas_model_realization gravitas_gltf_importer)
add_test(NAME gts_model_realization COMMAND GtsModelRealizationTest)
set_tests_properties(gts_model_realization PROPERTIES LABELS "gravitas;assets;realization;cpu")

add_executable(GtsModelMaterialRealizationTest ../rendering/material/GtsModelMaterialRealizationTest.cpp)
target_link_libraries(GtsModelMaterialRealizationTest PRIVATE gravitas_model_materials gravitas_gltf_importer gravitas_image_decode)
add_test(NAME gts_model_material_realization COMMAND GtsModelMaterialRealizationTest)
set_tests_properties(gts_model_material_realization PROPERTIES LABELS "gravitas;materials;cpu")

add_executable(GtsModelInstanceTest ../model/GtsModelInstanceTest.cpp)
target_link_libraries(GtsModelInstanceTest PRIVATE gravitas_model_instances gravitas_gltf_importer)
add_test(NAME gts_model_instance COMMAND GtsModelInstanceTest)
set_tests_properties(gts_model_instance PROPERTIES LABELS "gravitas;model;runtime;cpu")
