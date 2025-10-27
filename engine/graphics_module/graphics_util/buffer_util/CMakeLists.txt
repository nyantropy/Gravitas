file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(bufferutil ${CURRENT_DIR_SRC})

target_link_libraries(bufferutil PUBLIC graphicsutil graphics)

target_include_directories(bufferutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")