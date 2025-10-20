add_library(
    swapchain

    VulkanSwapChain.hpp
    VulkanSwapChain.cpp

    VulkanSwapChainConfig.h
)

target_include_directories(swapchain PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(swapchain PUBLIC instance physicaldevice output)