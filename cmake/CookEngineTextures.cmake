if(NOT DEFINED ASSETC OR NOT EXISTS "${ASSETC}")
    message(FATAL_ERROR "ASSETC must point to the assetc executable.")
endif()

if(NOT DEFINED RESOURCE_DIR OR NOT IS_DIRECTORY "${RESOURCE_DIR}")
    message(FATAL_ERROR "RESOURCE_DIR must point to the engine resources directory.")
endif()

function(cook_engine_texture texture_asset texture_role single_mip)
    get_filename_component(texture_output_dir "${texture_asset}" DIRECTORY)
    set(assetc_args
        "${ASSETC}"
        import
        "${texture_asset}"
        --output
        "${texture_output_dir}"
        --texture-role
        "${texture_role}"
    )
    if(single_mip)
        list(APPEND assetc_args --single-mip)
    endif()

    execute_process(
        COMMAND ${assetc_args}
        RESULT_VARIABLE assetc_result
        OUTPUT_VARIABLE assetc_stdout
        ERROR_VARIABLE assetc_stderr
    )

    if(NOT assetc_result EQUAL 0)
        message(FATAL_ERROR
            "assetc failed for engine texture ${texture_asset}\n"
            "stdout:\n${assetc_stdout}\n"
            "stderr:\n${assetc_stderr}")
    endif()
endfunction()

file(GLOB_RECURSE font_textures
    LIST_DIRECTORIES false
    "${RESOURCE_DIR}/fonts/*.png"
    "${RESOURCE_DIR}/fonts/*.jpg"
    "${RESOURCE_DIR}/fonts/*.jpeg"
)
list(SORT font_textures)
foreach(texture_asset IN LISTS font_textures)
    cook_engine_texture("${texture_asset}" "font-atlas" TRUE)
endforeach()

set(ui_textures
    "${RESOURCE_DIR}/textures/engine_ui_fallback.png"
)
set(particle_textures
    "${RESOURCE_DIR}/textures/engine_particle_fallback.png"
)
set(metallic_roughness_textures
    "${RESOURCE_DIR}/textures/engine_pbr_metallic_roughness_neutral.png"
    "${RESOURCE_DIR}/textures/engine_pbr_validation_metallic_roughness.png"
)
set(normal_textures
    "${RESOURCE_DIR}/textures/engine_pbr_normal_flat.png"
    "${RESOURCE_DIR}/textures/engine_pbr_validation_normal.png"
)
set(ao_textures
    "${RESOURCE_DIR}/textures/engine_pbr_ao_white.png"
    "${RESOURCE_DIR}/textures/engine_pbr_validation_ao.png"
)
set(emissive_textures
    "${RESOURCE_DIR}/textures/engine_pbr_emissive_black.png"
    "${RESOURCE_DIR}/textures/engine_pbr_validation_emissive.png"
)

foreach(texture_asset IN LISTS ui_textures)
    if(EXISTS "${texture_asset}")
        cook_engine_texture("${texture_asset}" "ui" TRUE)
    endif()
endforeach()

foreach(texture_asset IN LISTS particle_textures)
    if(EXISTS "${texture_asset}")
        cook_engine_texture("${texture_asset}" "particle" FALSE)
    endif()
endforeach()

foreach(texture_asset IN LISTS metallic_roughness_textures)
    if(EXISTS "${texture_asset}")
        cook_engine_texture("${texture_asset}" "metallic-roughness" FALSE)
    endif()
endforeach()

foreach(texture_asset IN LISTS normal_textures)
    if(EXISTS "${texture_asset}")
        cook_engine_texture("${texture_asset}" "normal" FALSE)
    endif()
endforeach()

foreach(texture_asset IN LISTS ao_textures)
    if(EXISTS "${texture_asset}")
        cook_engine_texture("${texture_asset}" "ambient-occlusion" FALSE)
    endif()
endforeach()

foreach(texture_asset IN LISTS emissive_textures)
    if(EXISTS "${texture_asset}")
        cook_engine_texture("${texture_asset}" "emissive" FALSE)
    endif()
endforeach()

set(non_color_texture_roles
    ${ui_textures}
    ${particle_textures}
    ${metallic_roughness_textures}
    ${normal_textures}
    ${ao_textures}
    ${emissive_textures}
)

file(GLOB_RECURSE color_textures
    LIST_DIRECTORIES false
    "${RESOURCE_DIR}/textures/*.png"
    "${RESOURCE_DIR}/textures/*.jpg"
    "${RESOURCE_DIR}/textures/*.jpeg"
)
list(SORT color_textures)
foreach(texture_asset IN LISTS color_textures)
    list(FIND non_color_texture_roles "${texture_asset}" existing_role_index)
    if(existing_role_index GREATER -1)
        continue()
    endif()
    cook_engine_texture("${texture_asset}" "base-color" FALSE)
endforeach()
