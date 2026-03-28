file(GLOB SCRIPT_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)
file(GLOB SCRIPT_SYSTEMS
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems/*.hpp"
)

add_library(ecsscript ${SCRIPT_COMPONENTS} ${SCRIPT_SYSTEMS})

target_include_directories(ecsscript PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
    "${CMAKE_CURRENT_SOURCE_DIR}/systems"
)
target_link_libraries(ecsscript PUBLIC ecsworld)
