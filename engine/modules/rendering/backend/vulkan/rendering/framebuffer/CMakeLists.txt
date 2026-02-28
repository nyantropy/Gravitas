file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(framebuffer ${CURRENT_DIR_SRC})

target_link_libraries(framebuffer PUBLIC rendering)

target_include_directories(framebuffer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")