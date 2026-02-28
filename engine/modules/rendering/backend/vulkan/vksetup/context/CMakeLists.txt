file(GLOB CURRENT_DIR_SRC ./*.hpp ./*.cpp ./*.c ./*.h)
add_library(context ${CURRENT_DIR_SRC})

target_include_directories(context PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(context PUBLIC instance surface logicaldevice physicaldevice swapchain)