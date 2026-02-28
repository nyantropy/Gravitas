file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(swapchain ${CURRENT_DIR_SRC})

target_include_directories(swapchain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(swapchain PUBLIC instance physicaldevice output)