add_library(texture INTERFACE)

target_include_directories(texture INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(texture INTERFACE vulkantexture descriptorset)
