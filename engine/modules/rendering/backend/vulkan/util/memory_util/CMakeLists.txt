file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(memoryutil ${CURRENT_DIR_SRC})

target_link_libraries(memoryutil PUBLIC graphicsutil)

target_include_directories(memoryutil PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")