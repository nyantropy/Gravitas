file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(time ${CURRENT_DIR_SRC})

target_link_libraries(time PUBLIC core)
target_include_directories(time PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")