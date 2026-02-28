file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(renderpass ${CURRENT_DIR_SRC})

target_link_libraries(renderpass PUBLIC rendering)

target_include_directories(renderpass PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")