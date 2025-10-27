file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(gcomponents ${CURRENT_DIR_SRC})

target_include_directories(gcomponents PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gcomponents PUBLIC graphics texture mesh)