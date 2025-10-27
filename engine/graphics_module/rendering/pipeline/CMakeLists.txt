file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(pipeline ${CURRENT_DIR_SRC})

target_link_libraries(pipeline PUBLIC rendering)

target_include_directories(pipeline PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")