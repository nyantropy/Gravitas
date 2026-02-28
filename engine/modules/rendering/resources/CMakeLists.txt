add_library(renderresource INTERFACE)

add_subdirectory(descriptorset)
add_subdirectory(mesh)
add_subdirectory(texture)
add_subdirectory(camera)
add_subdirectory(ssbo)

target_link_libraries(renderresource INTERFACE descriptorset mesh texture camera ssbo)

target_include_directories(renderresource INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")