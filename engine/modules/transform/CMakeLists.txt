file(GLOB TRANSFORM_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)

add_library(ecstransform ${TRANSFORM_COMPONENTS})

target_include_directories(ecstransform PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)
target_link_libraries(ecstransform PUBLIC graphics)
