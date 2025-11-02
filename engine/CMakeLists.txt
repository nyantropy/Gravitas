file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(GravitasEngine ${CURRENT_DIR_SRC})

target_include_directories(GravitasEngine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(GravitasEngine PUBLIC graphics core scene)

add_subdirectory(physics)
add_subdirectory(graphics_module)
add_subdirectory(core)