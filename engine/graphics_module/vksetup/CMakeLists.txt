add_library(vksetup INTERFACE)

add_subdirectory(instance)
add_subdirectory(physicaldevice)
add_subdirectory(logicaldevice)
add_subdirectory(texture)
add_subdirectory(context)
add_subdirectory(swapchain)

target_link_libraries(vksetup INTERFACE context instance physicaldevice logicaldevice texture swapchain)

target_include_directories(vksetup INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")