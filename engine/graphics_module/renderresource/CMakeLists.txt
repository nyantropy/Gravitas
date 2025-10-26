add_library(renderresource INTERFACE)

add_subdirectory(descriptorset)
add_subdirectory(mesh)
add_subdirectory(texture)
add_subdirectory(uniformbuffer)

target_link_libraries(renderresource INTERFACE descriptorset mesh texture uniformbuffer)

target_include_directories(renderresource INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")