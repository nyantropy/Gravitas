file(GLOB ANIMATION_COMPONENTS
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.h"
    "${CMAKE_CURRENT_SOURCE_DIR}/components/*.hpp"
)

add_library(ecsanimation ${ANIMATION_COMPONENTS})

target_include_directories(ecsanimation PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/components"
)
target_link_libraries(ecsanimation PUBLIC graphics)
