add_library(
    surface
    
    WindowSurface.hpp
    GLFWWindowSurface.hpp
    GLFWWindowSurface.cpp

    WindowSurfaceConfig.h
)

target_include_directories(surface PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

target_link_libraries(surface PUBLIC instance)