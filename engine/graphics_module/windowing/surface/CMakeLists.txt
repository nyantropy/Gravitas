file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(surface ${CURRENT_DIR_SRC})

target_include_directories(surface PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(surface PUBLIC instance)