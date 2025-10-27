file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(event ${CURRENT_DIR_SRC})

target_link_libraries(event PUBLIC graphics)

target_include_directories(event PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")