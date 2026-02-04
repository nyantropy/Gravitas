file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(rng ${CURRENT_DIR_SRC})

target_link_libraries(rng PUBLIC core)
target_include_directories(rng PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")