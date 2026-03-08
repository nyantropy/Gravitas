add_library(vksetup INTERFACE)

add_subdirectory(instance)
add_subdirectory(surface)
add_subdirectory(physicaldevice)
add_subdirectory(logicaldevice)
add_subdirectory(context)
add_subdirectory(swapchain)

target_link_libraries(vksetup INTERFACE context instance surface physicaldevice logicaldevice swapchain)

target_include_directories(vksetup INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")