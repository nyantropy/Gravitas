file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(framesyncobjects ${CURRENT_DIR_SRC})

target_link_libraries(framesyncobjects PUBLIC rendering)
target_include_directories(framesyncobjects PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

