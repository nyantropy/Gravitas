file(GLOB HIERARCHY_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)

add_library(ecshierarchy ${HIERARCHY_COMPONENTS})

target_include_directories(ecshierarchy PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)
target_link_libraries(ecshierarchy PUBLIC ecsworld)
