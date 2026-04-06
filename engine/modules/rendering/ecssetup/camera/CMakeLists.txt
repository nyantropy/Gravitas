file(GLOB CAMERA_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)
file(GLOB CAMERA_SYSTEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.hpp"
)
file(GLOB CAMERA_DEBUG
    "${CMAKE_CURRENT_SOURCE_DIR}/debug/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/debug/*.hpp"
)

add_library(ecscamera ${CAMERA_COMPONENTS} ${CAMERA_SYSTEMS} ${CAMERA_DEBUG})

target_include_directories(ecscamera PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/debug"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
)
target_link_libraries(ecscamera PUBLIC ecsworld ecstransform graphics)
