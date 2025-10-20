add_library(
    pipeline

    VulkanPipeline.hpp
    VulkanPipeline.cpp

    VulkanPipelineConfig.h
)

target_link_libraries(pipeline PUBLIC rendering)

target_include_directories(pipeline PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")