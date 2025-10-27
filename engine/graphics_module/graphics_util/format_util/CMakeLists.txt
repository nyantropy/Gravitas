file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(formatutil ${CURRENT_DIR_SRC})

target_link_libraries(formatutil PUBLIC graphicsutil)

target_include_directories(formatutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")