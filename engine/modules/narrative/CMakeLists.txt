# Narrative owns both compartments; dialogue remains available without rendering.
add_subdirectory(dialogue)
add_subdirectory(visualnovel/contracts)

if(GTS_ENABLE_RENDERING)
    add_subdirectory(visualnovel)
endif()
