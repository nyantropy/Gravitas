file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(output ${CURRENT_DIR_SRC})

target_link_libraries(output PUBLIC graphics input)

target_include_directories(output PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")