add_library(graphicsutil INTERFACE)

add_subdirectory(image_util)
add_subdirectory(memory_util)
add_subdirectory(format_util)

target_link_libraries(graphicsutil INTERFACE imageutil memoryutil formatutil)

target_include_directories(graphicsutil INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")