add_library(
    context

    VulkanContext.hpp
    VulkanContext.cpp

    VulkanContextConfig.h
)

target_include_directories(context PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(context PUBLIC instance logicaldevice physicaldevice)