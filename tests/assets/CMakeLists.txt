add_executable(GtsModelAssetTest GtsModelAssetTest.cpp)
target_link_libraries(GtsModelAssetTest PRIVATE gravitas_core)
add_test(NAME gts_model_asset COMMAND GtsModelAssetTest)
set_tests_properties(gts_model_asset PROPERTIES LABELS "gravitas;assets;cpu")
