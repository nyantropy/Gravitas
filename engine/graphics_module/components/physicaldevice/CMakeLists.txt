add_library(
    physicaldevice

    VulkanPhysicalDevice.hpp
    VulkanPhysicalDevice.cpp

    QueueFamilyIndices.h
    SwapChainSupportDetails.h
    VulkanPhysicalDeviceConfig.h
)

target_include_directories(physicaldevice PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(physicaldevice PUBLIC instance options)