add_library(
    core

    ComponentStorage.hpp
    ComponentStorage.cpp

    Entity.h

    System.cpp
    System.hpp
)

target_include_directories(core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(core PUBLIC Gravitas)

add_subdirectory(ecs)