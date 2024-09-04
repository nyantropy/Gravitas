add_library(
    instance

    VulkanInstance.hpp
    VulkanInstance.cpp

    VulkanInstanceFactory.hpp
    VulkanInstanceFactory.cpp

)

target_include_directories(instance PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

#add_subdirectory(surface)

target_link_libraries(instance PUBLIC options)

# target_include_directories(instance PUBLIC
#     "${CMAKE_CURRENT_SOURCE_DIR}"         # Include instance directory
# )
