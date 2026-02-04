file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(ecssystems ${CURRENT_DIR_SRC})

target_link_libraries(ecssystems PUBLIC core components)
target_include_directories(ecssystems PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")