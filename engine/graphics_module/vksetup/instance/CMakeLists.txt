add_library(
    instance

    VulkanInstance.hpp
    VulkanInstance.cpp

    VulkanInstanceConfig.h

)

target_include_directories(instance PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
#target_link_libraries(instance PUBLIC options)