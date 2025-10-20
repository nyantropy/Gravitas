add_library(
    output

    GLFWOutputWindow.hpp
    GLFWOutputWindow.cpp

    OutputWindow.hpp

    OutputWindowConfig.h
)

target_link_libraries(output PUBLIC Gravitas)

target_include_directories(output PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")