file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(vkshader ${CURRENT_DIR_SRC})

target_link_libraries(vkshader PUBLIC rendering)

target_include_directories(vkshader PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")