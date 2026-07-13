add_subdirectory(GtsScene1)
add_subdirectory(GtsScene2)
add_subdirectory(GtsScene3)
add_subdirectory(GtsPbrValidation)
add_subdirectory(GtsRenderingBenchmarks)

if(TARGET assetc)
    add_custom_target(cook_engine_textures
        COMMAND ${CMAKE_COMMAND}
        "-DASSETC=$<TARGET_FILE:assetc>"
        "-DRESOURCE_DIR=${GTS_ENGINE_ROOT}/resources"
        -P "${GTS_ENGINE_ROOT}/cmake/CookEngineTextures.cmake"
        DEPENDS assetc
    )

    foreach(engine_app IN ITEMS
            GtsScene1
            GtsScene2
            GtsScene3
            GtsPbrValidation
            GtsRenderingBenchmarks)
        if(TARGET "${engine_app}")
            add_dependencies("${engine_app}" cook_engine_textures)
        endif()
    endforeach()
endif()
