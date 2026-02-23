file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(gpucomponent ${CURRENT_DIR_SRC})

target_include_directories(gpucomponent PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gpucomponent PUBLIC graphics)