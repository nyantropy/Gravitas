# Collect all source files in the instance directory
file(GLOB INSTANCE_SOURCES "*.cpp")
file(GLOB INSTANCE_HEADERS "*.hpp")

# Create a static library for the instance module
add_library(instance STATIC ${INSTANCE_SOURCES} ${INSTANCE_HEADERS})

# Include directories for the instance module
target_include_directories(instance PUBLIC ${CMAKE_SOURCE_DIR}/graphics_engine/instance)
