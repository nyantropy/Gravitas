file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(ecs ${CURRENT_DIR_SRC})

target_link_libraries(ecs PUBLIC core)
target_include_directories(ecs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")