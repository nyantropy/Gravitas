file(GLOB_RECURSE CURRENT_DIR_SRC CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.h"
)
add_library(scene ${CURRENT_DIR_SRC})

target_link_libraries(scene PUBLIC core ecsrendering ecscamera ecstext time gtscommand options)
target_include_directories(scene PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
