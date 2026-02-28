file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(attachment ${CURRENT_DIR_SRC})

target_link_libraries(attachment PUBLIC rendering)

target_include_directories(attachment PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")