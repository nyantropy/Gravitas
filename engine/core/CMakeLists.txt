file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(core ${CURRENT_DIR_SRC})

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(core PUBLIC graphics)

add_subdirectory(ecs)
add_subdirectory(options)
add_subdirectory(event)
add_subdirectory(scene)
add_subdirectory(input)
add_subdirectory(rng)
add_subdirectory(time)
add_subdirectory(command)