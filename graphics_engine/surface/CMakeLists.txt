add_library(
    surface
    WindowSurface.hpp
    GLFWWindowSurface.hpp
    GLFWWindowSurface.cpp
)

target_include_directories(surface PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")