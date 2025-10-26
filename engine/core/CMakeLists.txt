file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(core ${CURRENT_DIR_SRC})

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(core PUBLIC Gravitas)

add_subdirectory(ecs)