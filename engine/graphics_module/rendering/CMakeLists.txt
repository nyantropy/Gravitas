add_library(
    rendering

    ForwardRenderer.hpp
    ForwardRenderer.cpp

    Renderer.hpp

    RendererConfig.h
)

target_link_libraries(rendering PUBLIC Gravitas attachment renderpass)

target_include_directories(rendering PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")

add_subdirectory(attachment)
add_subdirectory(renderpass)