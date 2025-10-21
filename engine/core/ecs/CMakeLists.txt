add_library(
    ecs

    ECSWorld.hpp
    ECSWorld.cpp
)

target_link_libraries(ecs PUBLIC core)

target_include_directories(ecs PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")