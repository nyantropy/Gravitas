add_library(modules INTERFACE)
target_link_libraries(modules INTERFACE ecsrendering ecscamera ecstransform ecstext ecsanimation ecsscript ecshierarchy)

# hierarchy first — ecsrendering depends on ecshierarchy for world-matrix computation
add_subdirectory(hierarchy)
# rendering second — defines the `graphics` target that all other modules depend on
add_subdirectory(rendering)
add_subdirectory(transform)
add_subdirectory(animation)
add_subdirectory(script)
add_subdirectory(camera)
add_subdirectory(text)
