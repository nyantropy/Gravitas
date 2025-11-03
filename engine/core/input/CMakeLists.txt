file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(input ${CURRENT_DIR_SRC})

target_link_libraries(input PUBLIC core)
target_include_directories(input PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")