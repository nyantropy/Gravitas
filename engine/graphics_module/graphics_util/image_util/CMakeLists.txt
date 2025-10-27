file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(imageutil ${CURRENT_DIR_SRC})

target_link_libraries(imageutil PUBLIC graphicsutil)

target_include_directories(imageutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")