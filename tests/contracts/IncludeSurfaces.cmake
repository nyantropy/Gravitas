# Each translation unit receives only its subject target's usage requirements.
function(add_include_surface_test name target)
    cmake_parse_arguments(surface "RUNTIME;BACKEND;FEATURE;IMPLEMENTATION" "" "HEADERS;FORBIDDEN" ${ARGN})
    if(NOT TARGET ${target})
        return()
    endif()
    set(source "")
    foreach(header IN LISTS surface_HEADERS)
        string(APPEND source "#include \"${header}\"\n")
    endforeach()
    set(forbidden ${surface_FORBIDDEN})
    if(NOT surface_RUNTIME)
        list(APPEND forbidden GravitasEngine.hpp BuiltinExecutionGroups.h SceneExecutionPolicy.h
            runtime/GravitasEngine.hpp runtime/execution/BuiltinExecutionGroups.h)
    endif()
    if(NOT surface_BACKEND)
        list(APPEND forbidden VulkanGraphics.hpp VulkanContext.hpp VulkanBackendContext.h
            VulkanSkinPaletteBuffer.h GtsDebugOverlay.h
            rendering/backend/vulkan/graphics/VulkanGraphics.hpp
            backend/vulkan/graphics/VulkanGraphics.hpp)
    endif()
    if(NOT surface_IMPLEMENTATION)
        list(APPEND forbidden EngineToolRuntime.hpp VNSystem.hpp PhysicsSystem.hpp PhysicsSystem.h
            RenderGpuSystem.hpp RenderingRuntime.h UiSystem.h
            tools/runtime/EngineToolRuntime.hpp narrative/visualnovel/systems/VNSystem.hpp)
    endif()
    if(NOT surface_FEATURE)
        list(APPEND forbidden ResourceTypes.h IGtsPhysicsModule.h UiSurface.h GtsModelControllerContext.h)
    endif()
    foreach(header IN LISTS forbidden)
        string(APPEND source "#if __has_include(\"${header}\")\n#error \"${target} exposes ${header}\"\n#endif\n")
    endforeach()
    string(APPEND source "int main() { return 0; }\n")
    set(file "${CMAKE_CURRENT_BINARY_DIR}/include-surfaces/${name}.cpp")
    file(GENERATE OUTPUT "${file}" CONTENT "${source}")
    add_executable(IncludeSurface_${name} "${file}")
    target_link_libraries(IncludeSurface_${name} PRIVATE ${target})
    add_test(NAME include_surface_${name} COMMAND IncludeSurface_${name})
    set_tests_properties(include_surface_${name} PROPERTIES LABELS "gravitas;contracts;includes;cpu")
endfunction()

add_include_surface_test(core gravitas_core HEADERS EcsControllerContext.hpp GtsScene.hpp InputWriter.hpp)
add_include_surface_test(model_controller gravitas_model_controller_contracts FEATURE
    HEADERS GtsModelControllerContext.h FORBIDDEN GtsModelRegistry.h GtsModelInstance.h GtsModelAsset.h
        loading/GtsModelRegistry.h runtime/GtsModelInstance.h model/loading/GtsModelRegistry.h)
add_include_surface_test(model_domain gravitas_model_domain FEATURE
    HEADERS GtsModelAsset.h GtsModelValidation.h FORBIDDEN GtsModelRegistry.h GtsModelInstance.h)
add_include_surface_test(image_decode gravitas_image_decode FEATURE
    HEADERS GtsImageDecode.h FORBIDDEN stb_image.h GtsModelRegistry.h AssetCooker.h)
add_include_surface_test(cooked_assets gravitas_cooked_assets FEATURE
    HEADERS AssetSerializers.h MeshAssetLoader.h FORBIDDEN AssetCooker.h GtsModelRegistry.h)
add_include_surface_test(skeletal gravitas_skeletal_animation FEATURE
    HEADERS GtsAnimationPlayback.h GtsAnimationSampling.h FORBIDDEN GtsModelRegistry.h GtsSkinPalette.h)
add_include_surface_test(skinning gravitas_skin_palette FEATURE HEADERS GtsSkinPalette.h GtsSkinPaletteEvaluation.h)
add_include_surface_test(model_extraction gravitas_model_frontend FEATURE
    HEADERS ModelFrameExtraction.h WorldTransformComponent.h FORBIDDEN VulkanGraphics.hpp)
add_include_surface_test(rendering_contracts gravitas_rendering_contracts FEATURE
    HEADERS ResourceTypes.h GtsFrameEndedEvent.h FORBIDDEN IResourceProvider.hpp GtsModelRegistry.h)
add_include_surface_test(rendering_commands gravitas_rendering_command_contracts FEATURE HEADERS ScreenshotCommand.h)
add_include_surface_test(rendering_execution gravitas_rendering_execution_contracts FEATURE HEADERS RendererExecutionInputs.h)
add_include_surface_test(rendering_controller gravitas_rendering_controller_contracts FEATURE HEADERS RenderingControllerContext.h)
add_include_surface_test(rendering_window gravitas_rendering_window_contracts FEATURE
    HEADERS OutputWindow.hpp PresentationSettings.h FORBIDDEN GLFWWindow.hpp IResourceProvider.hpp)
add_include_surface_test(physics gravitas_physics_contracts FEATURE
    HEADERS IGtsPhysicsModule.h ScenePhysics.h FORBIDDEN PhysicsWorld.h TransformSystem.hpp)
add_include_surface_test(ui gravitas_ui FEATURE HEADERS UiControllerContext.h UiSurface.h FORBIDDEN IResourceProvider.hpp)
add_include_surface_test(vn gravitas_vn_execution_contracts FEATURE HEADERS VNExecutionInputs.h FORBIDDEN VNRuntime.h)
add_include_surface_test(materials gravitas_material_frontend FEATURE HEADERS MaterialRuntime.h IResourceProvider.hpp)
add_include_surface_test(rendering gravitas_rendering FEATURE IMPLEMENTATION HEADERS RenderingRuntime.h RendererSceneFeature.h)
add_include_surface_test(vulkan_setup gravitas_vulkan_setup FEATURE BACKEND
    HEADERS VulkanContext.hpp OutputWindow.hpp GtsPlatformEventBus.hpp PresentationSettings.h
    FORBIDDEN VulkanGraphics.hpp VulkanBackendContext.h RenderingRuntime.h)
add_include_surface_test(vulkan_backend gravitas_vulkan_backend FEATURE
    FORBIDDEN VulkanGraphics.hpp VulkanContext.hpp RenderingRuntime.h)
add_include_surface_test(runtime gravitas_runtime FEATURE IMPLEMENTATION RUNTIME HEADERS SceneExecutionPolicy.h)

add_include_surface_test(tool_contracts gravitas_tool_contracts FEATURE HEADERS ToolSettings.h)
add_include_surface_test(profiling gravitas_profiling FEATURE HEADERS ISceneFrameStats.h)
if(TARGET gravitas_runtime)
    add_include_surface_test(facade gravitas_runtime FEATURE IMPLEMENTATION RUNTIME HEADERS GravitasEngine.hpp)
endif()
foreach(case allowed module_root wrapped_module_root source_root ancestor rendering_root
        sibling_allowed symlink_module_root root_alias)
    add_test(NAME public_includes_${case}
        COMMAND "${CMAKE_COMMAND}" "-DENGINE_SOURCE=${GTS_ENGINE_ROOT}"
            "-DTEST_BINARY_ROOT=${CMAKE_CURRENT_BINARY_DIR}" "-DCASE=${case}"
            -P "${GTS_ENGINE_ROOT}/cmake/TestPublicIncludes.cmake")
endforeach()

add_include_surface_test(asset_contracts gravitas_asset_contracts FEATURE
    HEADERS GtsStaticVertex.h MaterialInstanceHandle.h
    FORBIDDEN AssetSerializers.h RuntimeMeshLoading.h GtsModelRegistry.h)
