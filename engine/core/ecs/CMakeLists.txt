add_library(ecs INTERFACE)

target_link_libraries(ecs
    INTERFACE
        ecsworld
        ecssystems
        ecsextractors
        components
)

target_include_directories(ecs
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)

add_subdirectory(ecssystems)
add_subdirectory(ecsextractors)
add_subdirectory(components)
add_subdirectory(ecsworld)