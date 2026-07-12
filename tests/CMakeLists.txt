add_executable(GravitasHeadlessRuntimeSmoke
    runtime/HeadlessRuntimeSmoke.cpp
)

target_compile_features(GravitasHeadlessRuntimeSmoke PRIVATE cxx_std_20)

target_link_libraries(GravitasHeadlessRuntimeSmoke PRIVATE gravitas_engine)

add_test(
    NAME gravitas_runtime_headless_startup
    COMMAND GravitasHeadlessRuntimeSmoke
)

set_tests_properties(gravitas_runtime_headless_startup
    PROPERTIES
        LABELS "gravitas;runtime;smoke"
        SKIP_RETURN_CODE 77
)

add_executable(GravitasHeadlessScreenshotSmoke
    runtime/HeadlessScreenshotRuntimeSmoke.cpp
)

target_compile_features(GravitasHeadlessScreenshotSmoke PRIVATE cxx_std_20)

target_link_libraries(GravitasHeadlessScreenshotSmoke PRIVATE gravitas_engine)

add_test(
    NAME gravitas_runtime_headless_screenshot
    COMMAND GravitasHeadlessScreenshotSmoke
)

set_tests_properties(gravitas_runtime_headless_screenshot
    PROPERTIES
        LABELS "gravitas;runtime;smoke;screenshot"
        SKIP_RETURN_CODE 77
)

add_executable(ProfileAccumulatorTest
    core/ProfileAccumulatorTest.cpp
)

target_compile_features(ProfileAccumulatorTest PRIVATE cxx_std_20)

target_link_libraries(ProfileAccumulatorTest PRIVATE gravitas_engine)

add_test(
    NAME profile_accumulator
    COMMAND ProfileAccumulatorTest
)

set_tests_properties(profile_accumulator
    PROPERTIES
        LABELS "gravitas;core;profiling"
)

add_executable(TransformHierarchyRuntimeTest
    transform/TransformHierarchyRuntimeTest.cpp
)

target_compile_features(TransformHierarchyRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(TransformHierarchyRuntimeTest PRIVATE gravitas_modules)

add_test(
    NAME transform_hierarchy_runtime
    COMMAND TransformHierarchyRuntimeTest
)

set_tests_properties(transform_hierarchy_runtime
    PROPERTIES
        LABELS "gravitas;transform;runtime"
)

add_executable(ParticleModuleAuthoringTest
    particles/ParticleModuleAuthoringTest.cpp
)

target_compile_features(ParticleModuleAuthoringTest PRIVATE cxx_std_20)

target_link_libraries(ParticleModuleAuthoringTest PRIVATE gravitas_rendering)

add_test(
    NAME particle_module_authoring
    COMMAND ParticleModuleAuthoringTest
)

set_tests_properties(particle_module_authoring
    PROPERTIES
        LABELS "gravitas;particles;authoring"
)

add_executable(RenderLifecycleOwnershipTest
    rendering/RenderLifecycleOwnershipTest.cpp
)

target_compile_features(RenderLifecycleOwnershipTest PRIVATE cxx_std_20)

target_link_libraries(RenderLifecycleOwnershipTest PRIVATE gravitas_rendering)

add_test(
    NAME render_lifecycle_ownership
    COMMAND RenderLifecycleOwnershipTest
)

set_tests_properties(render_lifecycle_ownership
    PROPERTIES
        LABELS "gravitas;rendering;lifecycle"
)

add_executable(MaterialRuntimeArchitectureTest
    rendering/MaterialRuntimeArchitectureTest.cpp
)

target_compile_features(MaterialRuntimeArchitectureTest PRIVATE cxx_std_20)

target_link_libraries(MaterialRuntimeArchitectureTest PRIVATE gravitas_rendering)

add_test(
    NAME material_runtime_architecture
    COMMAND MaterialRuntimeArchitectureTest
)

set_tests_properties(material_runtime_architecture
    PROPERTIES
        LABELS "gravitas;rendering;materials"
)

add_executable(GeometryPbrPreparationTest
    rendering/GeometryPbrPreparationTest.cpp
)

target_compile_features(GeometryPbrPreparationTest PRIVATE cxx_std_20)

target_link_libraries(GeometryPbrPreparationTest PRIVATE gravitas_vulkan_backend)

add_test(
    NAME geometry_pbr_preparation
    COMMAND GeometryPbrPreparationTest
)

set_tests_properties(geometry_pbr_preparation
    PROPERTIES
        LABELS "gravitas;rendering;geometry"
)

add_executable(ObjAssetImporterTest
    assets/ObjAssetImporterTest.cpp
)

target_compile_features(ObjAssetImporterTest PRIVATE cxx_std_20)

target_link_libraries(ObjAssetImporterTest PRIVATE gravitas_rendering)

add_test(
    NAME obj_asset_importer
    COMMAND ObjAssetImporterTest
)

set_tests_properties(obj_asset_importer
    PROPERTIES
        LABELS "gravitas;assets;importer"
)

add_executable(AssetSerializationTest
    assets/AssetSerializationTest.cpp
)

target_compile_features(AssetSerializationTest PRIVATE cxx_std_20)

target_link_libraries(AssetSerializationTest PRIVATE gravitas_rendering)

add_test(
    NAME asset_serialization
    COMMAND AssetSerializationTest
)

set_tests_properties(asset_serialization
    PROPERTIES
        LABELS "gravitas;assets;serialization"
)

add_executable(CookedAssetPipelineTest
    assets/CookedAssetPipelineTest.cpp
)

target_compile_features(CookedAssetPipelineTest PRIVATE cxx_std_20)

target_link_libraries(CookedAssetPipelineTest PRIVATE gravitas_vulkan_backend)

add_test(
    NAME cooked_asset_pipeline
    COMMAND CookedAssetPipelineTest
)

set_tests_properties(cooked_asset_pipeline
    PROPERTIES
        LABELS "gravitas;assets;cooked"
)

add_executable(FirstLitRendererArchitectureTest
    rendering/FirstLitRendererArchitectureTest.cpp
)

target_compile_features(FirstLitRendererArchitectureTest PRIVATE cxx_std_20)

target_link_libraries(FirstLitRendererArchitectureTest PRIVATE gravitas_vulkan_backend)

add_test(
    NAME first_lit_renderer_architecture
    COMMAND FirstLitRendererArchitectureTest
)

set_tests_properties(first_lit_renderer_architecture
    PROPERTIES
        LABELS "gravitas;rendering;lighting"
)

add_executable(ObjectMaterialColorOwnershipTest
    rendering/ObjectMaterialColorOwnershipTest.cpp
)

target_compile_features(ObjectMaterialColorOwnershipTest PRIVATE cxx_std_20)

target_compile_definitions(ObjectMaterialColorOwnershipTest
    PRIVATE
        GRAVITAS_ENGINE_ROOT="${GTS_ENGINE_ROOT}"
)

target_link_libraries(ObjectMaterialColorOwnershipTest PRIVATE gravitas_vulkan_backend)

add_test(
    NAME object_material_color_ownership
    COMMAND ObjectMaterialColorOwnershipTest
)

set_tests_properties(object_material_color_ownership
    PROPERTIES
        LABELS "gravitas;rendering;materials"
)

add_executable(RenderingBenchmarkSupportTest
    benchmarks/RenderingBenchmarkSupportTest.cpp
)

target_compile_features(RenderingBenchmarkSupportTest PRIVATE cxx_std_20)

target_link_libraries(RenderingBenchmarkSupportTest PRIVATE gravitas_rendering)

add_test(
    NAME rendering_benchmark_support
    COMMAND RenderingBenchmarkSupportTest
)

set_tests_properties(rendering_benchmark_support
    PROPERTIES
        LABELS "gravitas;rendering;benchmark;benchmark_smoke"
)

add_test(
    NAME rendering_benchmark_static_geometry_smoke
    COMMAND GtsRenderingBenchmarks
        --preset static_geometry_small
        --mode cpu_smoke
        --warmup-frames 2
        --measured-frames 4
        --renderable-count 16
        --visible-renderable-count 8
        --unique-mesh-count 2
        --unique-material-count 2
        --output "${CMAKE_CURRENT_BINARY_DIR}/static_geometry_small_smoke.json"
        --check-invariants
        --quiet
)

set_tests_properties(rendering_benchmark_static_geometry_smoke
    PROPERTIES
        LABELS "gravitas;rendering;benchmark;benchmark_smoke"
)

add_test(
    NAME rendering_benchmark_screenshot_capture_smoke
    COMMAND GtsRenderingBenchmarks
        --preset screenshot_capture_smoke
        --output "${CMAKE_CURRENT_BINARY_DIR}/screenshot_capture_smoke.json"
        --check-invariants
        --quiet
)

set_tests_properties(rendering_benchmark_screenshot_capture_smoke
    PROPERTIES
        LABELS "gravitas;rendering;benchmark;benchmark_gpu;screenshot"
        SKIP_RETURN_CODE 77
)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME rendering_benchmark_compare_tool
        COMMAND "${Python3_EXECUTABLE}"
            "${GTS_ENGINE_ROOT}/benchmarks/tools/compare_benchmark_results.py"
            --baseline "${GTS_ENGINE_ROOT}/benchmarks/fixtures/static_geometry_small_baseline.json"
            --current "${GTS_ENGINE_ROOT}/benchmarks/fixtures/static_geometry_small_current.json"
    )

    set_tests_properties(rendering_benchmark_compare_tool
        PROPERTIES
            LABELS "gravitas;rendering;benchmark;benchmark_smoke"
    )
endif()

add_executable(UiLayerRuntimeTest
    ui/UiLayerRuntimeTest.cpp
)

target_compile_features(UiLayerRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiLayerRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_layer_runtime
    COMMAND UiLayerRuntimeTest
)

set_tests_properties(ui_layer_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiInputDispatcherRuntimeTest
    ui/UiInputDispatcherRuntimeTest.cpp
)

target_compile_features(UiInputDispatcherRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiInputDispatcherRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_input_dispatcher_runtime
    COMMAND UiInputDispatcherRuntimeTest
)

set_tests_properties(ui_input_dispatcher_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiFocusManagerRuntimeTest
    ui/UiFocusManagerRuntimeTest.cpp
)

target_compile_features(UiFocusManagerRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiFocusManagerRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_focus_manager_runtime
    COMMAND UiFocusManagerRuntimeTest
)

set_tests_properties(ui_focus_manager_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiModalManagerRuntimeTest
    ui/UiModalManagerRuntimeTest.cpp
)

target_compile_features(UiModalManagerRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiModalManagerRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_modal_manager_runtime
    COMMAND UiModalManagerRuntimeTest
)

set_tests_properties(ui_modal_manager_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiMountRuntimeTest
    ui/UiMountRuntimeTest.cpp
)

target_compile_features(UiMountRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiMountRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_mount_runtime
    COMMAND UiMountRuntimeTest
)

set_tests_properties(ui_mount_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiCompositionRuntimeTest
    ui/UiCompositionRuntimeTest.cpp
)

target_compile_features(UiCompositionRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiCompositionRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_composition_runtime
    COMMAND UiCompositionRuntimeTest
)

set_tests_properties(ui_composition_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiEventPropagationRuntimeTest
    ui/UiEventPropagationRuntimeTest.cpp
)

target_compile_features(UiEventPropagationRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiEventPropagationRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_event_propagation_runtime
    COMMAND UiEventPropagationRuntimeTest
)

set_tests_properties(ui_event_propagation_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiSurfaceRuntimeTest
    ui/UiSurfaceRuntimeTest.cpp
)

target_compile_features(UiSurfaceRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiSurfaceRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_surface_runtime
    COMMAND UiSurfaceRuntimeTest
)

set_tests_properties(ui_surface_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiLayoutRuntimeTest
    ui/UiLayoutRuntimeTest.cpp
)

target_compile_features(UiLayoutRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiLayoutRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_layout_runtime
    COMMAND UiLayoutRuntimeTest
)

set_tests_properties(ui_layout_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiThemeRuntimeTest
    ui/UiThemeRuntimeTest.cpp
)

target_compile_features(UiThemeRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiThemeRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_theme_runtime
    COMMAND UiThemeRuntimeTest
)

set_tests_properties(ui_theme_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiWidgetRuntimeTest
    ui/UiWidgetRuntimeTest.cpp
)

target_compile_features(UiWidgetRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiWidgetRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_widget_runtime
    COMMAND UiWidgetRuntimeTest
)

set_tests_properties(ui_widget_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiNavigationRuntimeTest
    ui/UiNavigationRuntimeTest.cpp
)

target_compile_features(UiNavigationRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiNavigationRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_navigation_runtime
    COMMAND UiNavigationRuntimeTest
)

set_tests_properties(ui_navigation_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(VNFrontendRuntimeTest
    ui/VNFrontendRuntimeTest.cpp
)

target_compile_features(VNFrontendRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(VNFrontendRuntimeTest PRIVATE gravitas_engine)

add_test(
    NAME vn_frontend_runtime
    COMMAND VNFrontendRuntimeTest
)

set_tests_properties(vn_frontend_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime;vn"
)

add_executable(UiDragDropRuntimeTest
    ui/UiDragDropRuntimeTest.cpp
)

target_compile_features(UiDragDropRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiDragDropRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_drag_drop_runtime
    COMMAND UiDragDropRuntimeTest
)

set_tests_properties(ui_drag_drop_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiAnimationRuntimeTest
    ui/UiAnimationRuntimeTest.cpp
)

target_compile_features(UiAnimationRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiAnimationRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_animation_runtime
    COMMAND UiAnimationRuntimeTest
)

set_tests_properties(ui_animation_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiBindingRuntimeTest
    ui/UiBindingRuntimeTest.cpp
)

target_compile_features(UiBindingRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiBindingRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_binding_runtime
    COMMAND UiBindingRuntimeTest
)

set_tests_properties(ui_binding_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiAccessibilityRuntimeTest
    ui/UiAccessibilityRuntimeTest.cpp
)

target_compile_features(UiAccessibilityRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiAccessibilityRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_accessibility_runtime
    COMMAND UiAccessibilityRuntimeTest
)

set_tests_properties(ui_accessibility_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiSerializationRuntimeTest
    ui/UiSerializationRuntimeTest.cpp
)

target_compile_features(UiSerializationRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiSerializationRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_serialization_runtime
    COMMAND UiSerializationRuntimeTest
)

set_tests_properties(ui_serialization_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiWidgetAssetRuntimeTest
    ui/UiWidgetAssetRuntimeTest.cpp
)

target_compile_features(UiWidgetAssetRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiWidgetAssetRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_widget_asset_runtime
    COMMAND UiWidgetAssetRuntimeTest
)

set_tests_properties(ui_widget_asset_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiAssetRuntimeTest
    ui/UiAssetRuntimeTest.cpp
)

target_compile_features(UiAssetRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiAssetRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_asset_runtime
    COMMAND UiAssetRuntimeTest
)

set_tests_properties(ui_asset_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiPackageRuntimeTest
    ui/UiPackageRuntimeTest.cpp
)

target_compile_features(UiPackageRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiPackageRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_package_runtime
    COMMAND UiPackageRuntimeTest
)

set_tests_properties(ui_package_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)

add_executable(UiLocalizationRuntimeTest
    ui/UiLocalizationRuntimeTest.cpp
)

target_compile_features(UiLocalizationRuntimeTest PRIVATE cxx_std_20)

target_link_libraries(UiLocalizationRuntimeTest PRIVATE gravitas_rendering)

add_test(
    NAME ui_localization_runtime
    COMMAND UiLocalizationRuntimeTest
)

set_tests_properties(ui_localization_runtime
    PROPERTIES
        LABELS "gravitas;ui;runtime"
)
