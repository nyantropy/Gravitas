add_library(
    framebuffer

    VulkanFramebufferSet.hpp
    VulkanFramebufferSet.cpp

    VulkanFramebufferConfig.h

    VulkanFramebufferManager.cpp
    VulkanFramebufferManager.hpp

    VulkanFramebufferManagerConfig.h
)

target_link_libraries(framebuffer PUBLIC rendering)

target_include_directories(framebuffer PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")