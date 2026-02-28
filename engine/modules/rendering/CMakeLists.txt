file(GLOB RENDERING_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)
file(GLOB RENDERING_SYSTEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.hpp"
)
file(GLOB RENDERING_EXTRACTORS
    "${CMAKE_CURRENT_SOURCE_DIR}/extractors/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/extractors/*.hpp"
)

add_library(ecsrendering ${RENDERING_COMPONENTS} ${RENDERING_SYSTEMS} ${RENDERING_EXTRACTORS})

target_include_directories(ecsrendering PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
    "${CMAKE_CURRENT_SOURCE_DIR}/extractors"
)
target_link_libraries(ecsrendering PUBLIC ecsworld ecstransform ecscamera graphics)
