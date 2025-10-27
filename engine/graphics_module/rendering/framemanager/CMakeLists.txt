file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(framemanager ${CURRENT_DIR_SRC})

target_link_libraries(framemanager PUBLIC rendering options context)
target_include_directories(framemanager PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

