add_library(windowing INTERFACE)

add_subdirectory(windowmanager)
add_subdirectory(output)
add_subdirectory(surface)
add_subdirectory(glfwWindow)

target_link_libraries(windowing INTERFACE glfwWindow output surface windowmanager)

target_include_directories(windowing INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}")