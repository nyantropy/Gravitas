add_library(
    texture

    VulkanTexture.hpp
    VulkanTexture.cpp
)

target_include_directories(texture PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(texture PUBLIC physicaldevice logicaldevice external)