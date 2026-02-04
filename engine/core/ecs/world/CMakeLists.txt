file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(ecsworld ${CURRENT_DIR_SRC})

target_link_libraries(ecsworld PUBLIC core ecssystems)
target_include_directories(ecsworld PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")