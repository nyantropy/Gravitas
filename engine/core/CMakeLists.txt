add_library(core INTERFACE)

target_link_libraries(core
    INTERFACE
        ecs
        options
        event
        scene
        input
        rng
        time
        gtscommand
)

target_include_directories(core
    INTERFACE
        ${CMAKE_CURRENT_SOURCE_DIR}
)


add_subdirectory(ecs)
add_subdirectory(options)
add_subdirectory(event)
add_subdirectory(scene)
add_subdirectory(input)
add_subdirectory(rng)
add_subdirectory(time)
add_subdirectory(command)