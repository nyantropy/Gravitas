file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(components ${CURRENT_DIR_SRC})

target_include_directories(components PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(components PUBLIC graphics texture mesh)

add_subdirectory(camera)
add_subdirectory(animation)