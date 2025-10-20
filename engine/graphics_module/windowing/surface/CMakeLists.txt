add_library(
    surface
    
    VulkanSurface.hpp
    GLFWVulkanSurface.hpp
    GLFWVulkanSurface.cpp

    VulkanSurfaceConfig.h
)

target_include_directories(surface PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(surface PUBLIC instance)