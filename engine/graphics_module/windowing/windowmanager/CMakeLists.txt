file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(windowmanager ${CURRENT_DIR_SRC})

target_include_directories(windowmanager PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(windowmanager PUBLIC output)