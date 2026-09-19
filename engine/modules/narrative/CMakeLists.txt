# Narrative owns both compartments; dialogue remains available without rendering.
add_subdirectory(dialogue)

if(GTS_ENABLE_RENDERING)
    add_subdirectory(visualnovel)
endif()
