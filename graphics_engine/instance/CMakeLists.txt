# Collect all source files in the instance directory
file(GLOB INSTANCE_SOURCES "*.cpp")
file(GLOB INSTANCE_HEADERS "*.hpp")

# Create a static library for the instance module
add_library(instance STATIC ${INSTANCE_SOURCES} ${INSTANCE_HEADERS})

# Include directories for the instance module
target_include_directories(instance PUBLIC 
    ${CMAKE_SOURCE_DIR}/graphics_engine/instance
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIRS}
    ${STB_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
)

# Link necessary libraries
target_link_libraries(instance
    Vulkan::Vulkan
    $<$<PLATFORM_ID:Windows>:${GLFW_LIB_DIR}/glfw3.lib> # Windows
    $<$<NOT:$<PLATFORM_ID:Windows>>:${GLFW_LIB_DIR}/libglfw3.a> # Linux
)

# Ensure the parent target Gravitas includes the instance library
add_dependencies(Gravitas instance)
