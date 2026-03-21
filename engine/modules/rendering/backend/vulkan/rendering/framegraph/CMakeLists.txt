file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(framegraph ${CURRENT_DIR_SRC})

target_link_libraries(framegraph PUBLIC rendering)

target_include_directories(framegraph PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
