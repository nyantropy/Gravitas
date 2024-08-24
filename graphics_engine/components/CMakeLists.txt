add_library(components INTERFACE)

add_subdirectory(instance)

target_link_libraries(components INTERFACE instance)

target_include_directories(components INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")