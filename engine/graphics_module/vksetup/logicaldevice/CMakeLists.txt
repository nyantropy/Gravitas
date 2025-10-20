add_library(
    logicaldevice

    VulkanLogicalDevice.hpp
    VulkanLogicalDevice.cpp
)

target_include_directories(logicaldevice PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(logicaldevice PUBLIC physicaldevice)