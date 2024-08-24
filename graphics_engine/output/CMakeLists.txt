add_library(
    output

    GTSGLFWOutputWindow.hpp
    GTSGLFWOutputWindow.cpp

    GTSOutputWindow.hpp
)

target_link_libraries(output PUBLIC Gravitas)

target_include_directories(output PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")