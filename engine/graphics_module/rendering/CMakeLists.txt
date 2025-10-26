file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(rendering ${CURRENT_DIR_SRC})

target_link_libraries(rendering PUBLIC Gravitas attachment renderpass pipeline vkshader framebuffer framesyncobjects options)
target_include_directories(rendering PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(attachment)
add_subdirectory(renderpass)
add_subdirectory(pipeline)
add_subdirectory(shader)
add_subdirectory(framebuffer)
add_subdirectory(framesyncobjects)