file(GLOB TEXT_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)
file(GLOB TEXT_SYSTEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.hpp"
)

add_library(ecstext ${TEXT_COMPONENTS} ${TEXT_SYSTEMS})

target_include_directories(ecstext PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
)
target_link_libraries(ecstext PUBLIC ecsworld ecstransform ecsrendering graphics renderresource mesh)
