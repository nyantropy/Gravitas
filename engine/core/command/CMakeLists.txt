file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(gtscommand ${CURRENT_DIR_SRC})

target_link_libraries(gtscommand PUBLIC core)
target_include_directories(gtscommand PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")