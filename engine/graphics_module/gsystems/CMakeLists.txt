add_library(
    gsystems

    RenderSystem.hpp
    RenderSystem.cpp
)

target_include_directories(gsystems PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(gsystems PUBLIC gcomponents ecs)